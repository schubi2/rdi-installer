#include "config.h"

#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <stdbool.h>
#include <curl/curl.h>

#include "basics.h"
#include "logger.h"
#include "nc-dialogs.h"
#include "rdii-menu.h"
#include "rdii-helper.h"
#include "rdii-select-online-image.h"
#include "download.h"

// Data structure for the architecture-specific items
typedef struct {
    char *name;
    char *arch; // "x86", "arm64", "s390"
} Image;

typedef struct {
    Image *data;
    size_t size;
    size_t capacity;
} ImageList;

// Initialize the list
static int init_list(ImageList *list, size_t initial_capacity) {
    list->size = 0;
    list->capacity = initial_capacity;
    list->data = malloc(list->capacity * sizeof(Image));
    if (!list->data) {
        perror("Failed to allocate image list");
        return -ENOMEM;
    }
    return 0;
}

// Add an element to the list (allocating memory for strings)
static int add_image(ImageList *list, const char *name, const char *arch) {
    // Resize the array if it reaches capacity
    if (list->size >= list->capacity) {
        list->capacity *= 2;
        Image *new_data = realloc(list->data, list->capacity * sizeof(Image));
        if (!new_data) {
            perror("Failed to reallocate image list");
            return -ENOMEM;            
        }
        list->data = new_data;
    }

    // Allocate memory and copy string content using strdup
    list->data[list->size].name = strdup(name);
    list->data[list->size].arch = strdup(arch);

    list->size++;
    return 0;
}

// Free all allocated memory within the list
static void free_list(ImageList *list) {
    for (size_t i = 0; i < list->size; i++) {
        free(list->data[i].name);
        free(list->data[i].arch);
    }
    free(list->data);
    list->data = NULL;
    list->size = 0;
    list->capacity = 0;
}

const char *ARCH_OPTIONS[] = {
    "all",
    "x86",
    "arm64",
    NULL
};

bool is_supported_image(const char *name)
{
  const char *exts[] = {
    ".img",     ".raw",
    ".img.gz",  ".raw.gz",
    ".img.bz2", ".raw.bz2",
    ".img.xz",  ".raw.xz",
    ".img.zst", ".raw.zst"
  };
  const size_t num_exts = sizeof(exts) / sizeof(exts[0]);

  for (size_t i = 0; i < num_exts; i++)
    if (endswith(name, exts[i]))
      return true;

  return false;
}

/*
 *  select an image from a remote SHA256SUMS listing
 */

/*
  Parse SHA256SUMS file and return the names of all supported images.
  Returns the number of found entries (>= 0), or a negative errno code.
*/
static int
parse_sha256sums(const char *path, ImageList *ret_images)
{
  _cleanup_fclose_ FILE *fp = NULL;
  _cleanup_free_ char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  int count = 0;

  MSG_FUNC("path='%s'", path);

  fp = fopen(path, "r");
  if (!fp)
    return -errno;

  while ((linelen = getline(&line, &linecap, fp)) > 0)
    {
      char *nl = strchr(line, '\n');
      if (nl)
	*nl = '\0';

      // A valid line is "<64 hex char hash><separator><filename>"
      if (linelen < 66)
	continue;

      char *name = strchr(line, ' ');
      while (*name == ' ')
	++name;

      if (isempty(name) || !is_supported_image(name))
	continue;
      int ret = add_image(ret_images, name, "x86");
      if (ret == 0)
        count++;
      else
        return ret;
    }          

  MSG_INFO("Found %i supported image(s) in '%s'", count, path);

  return count;
}

static char *choose_image(ImageList *image_list, const char *title)
{
  size_t selected = 0;
  const char *help_text = "Select raw disk image which has to be installed on the target system.\n"
    "The name of the default download server can be changed by the value of rdii.download_server, set "
    "by the kernel cmdline during boot or by a configuration file.\n";
  

  MSG_FUNC("number of images=%i", image_list->size);

  print_global_header_footer("F1: Help", SELECTION);
  print_title((title ? title : ""));

  while (1)
    {
      for (size_t i = 0; i < image_list->size; i++)
	{
	  int y = 4 + i;

	  if (i == selected)
	    {
	      attron(COLOR_PAIR(CP_SELECTED) | A_BOLD);
	      mvprintw(y, 2, "-> %s", image_list->data[i].name);
	      attroff(COLOR_PAIR(CP_SELECTED) | A_BOLD);
	    }
	  else
	    {
	      attron(COLOR_PAIR(CP_UNSELECTED));
	      mvprintw(y, 2, "   %s", image_list->data[i].name);
	      attroff(COLOR_PAIR(CP_UNSELECTED));
	    }
	}

      refresh();

      int ch = getch();
      if (ch == 27) // 27 is the ASCII code for ESC
	{
	  MSG_INFO("Canceld with ESC");
	  return NULL;
	}
      else if (ch == KEY_UP)
	selected = (selected - 1 + image_list->size) % image_list->size;
      else if (ch == KEY_DOWN)
	selected = (selected + 1) % image_list->size;
      else if (ch == KEY_F1)
        show_help_dialog(title, help_text);
      else if (ch == '\n' || ch == KEY_ENTER)
	{
	  MSG_INFO("Selected entry %i", selected);
	  return image_list->data[selected].name;
	}
    }

  MSG_ERROR("quit while loop without return!");

  // we should never reach this
  return NULL;
}


int get_url_from_list(char **ret)
{
  _cleanup_free_ char *sha256sums_url = NULL;
  _cleanup_free_ char *sha256sums_asc_url = NULL;
  _cleanup_free_ char *sha256sums_fn = NULL;
  _cleanup_free_ char *sha256sums_asc_fn = NULL;
  ImageList image_list;
  int num_images;
  char *selected_image;
  int r = 0;

  MSG_FUNC();

  
  r = init_list(&image_list, 2);
  if (r != 0)
    return r;

  if (asprintf(&sha256sums_url, "%s/SHA256SUMS", rdii_download_server) < 0)
    return -ENOMEM;
  if (asprintf(&sha256sums_asc_url, "%s/SHA256SUMS.asc", rdii_download_server) < 0)
    return -ENOMEM;
  if (asprintf(&sha256sums_fn, "%s/SHA256SUMS", rdii_tmp_dir) < 0)
    return -ENOMEM;
  if (asprintf(&sha256sums_asc_fn, "%s/SHA256SUMS.asc", rdii_tmp_dir) < 0)
    return -ENOMEM;

  r = curl_download_file(sha256sums_url, sha256sums_fn);
  if (r != 0)
    {
      MSG_ERROR("Error downloading SHA256SUMS file:",
		r < 0 ? strerror(-r) : curl_easy_strerror(r));
      show_error_popup("Error downloading SHA256SUMS file:",
		       r < 0 ? strerror(-r) : curl_easy_strerror(r), NULL);
      return -EIO;
    }

  r = curl_download_file(sha256sums_asc_url, sha256sums_asc_fn);
  if (r != 0)
    {
      MSG_ERROR("Error downloading SHA256SUMS.asc file:",
		r < 0 ? strerror(-r) : curl_easy_strerror(r));
      if (!show_warning_popup("Error downloading SHA256SUMS.asc file:",
			      r < 0 ? strerror(-r) : curl_easy_strerror(r),
			      "Continue without signature verification?"))
	return -ECANCELED;
    }
  else
    {
      char *error_msg = NULL;
      if (!verify_signature(sha256sums_fn, sha256sums_asc_fn, &error_msg))
	{
	  MSG_WARN("Cannot verify SHA256SUMS signature: %s", error_msg);
	  if (!show_warning_popup("Cannot verify signature.", error_msg,
				  "Continue without signature verification?"))
	    {
	      MSG_ERROR("Canceld");
	      return -ECANCELED;
	    }
	}
    }

  num_images = parse_sha256sums(sha256sums_fn, &image_list);
  if (num_images < 0)
    {
      MSG_ERROR("Error parsing SHA256SUMS file: %s",
		strerror(-num_images));
      show_error_popup("Error parsing SHA256SUMS file:",
		       strerror(-num_images), NULL);
      return num_images;
    }
  if (num_images == 0)
    {
      MSG_INFO("No supported images found in %s", rdii_download_server);
      show_error_popup("No supported images found in SHA256SUMS file.",
			NULL, NULL);
      return -ENOENT;
    }

  _cleanup_free_ char *header = NULL;

  if (asprintf(&header, "Select image from download server %s", rdii_download_server) < 0)
    {
      free_list(&image_list);      
      return -ENOMEM;
    }

  selected_image = choose_image(&image_list, header); 
  if (selected_image)
    {
      if (asprintf(ret, "%s/%s", rdii_download_server, selected_image) < 0)
        r = -ENOMEM;
    }
  else
    r = -1;

  free_list(&image_list);

  return r;
}
