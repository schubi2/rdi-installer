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
    const char *name;
    const char *arch; // "x86", "arm64", "s390"
} Item;

// Sample items list
Item ALL_ITEMS[] = {
    {"GCC Compiler Toolchain", "x86"},
    {"Clang LLVM Engine",     "x86"},
    {"QEMU x86 System Emulator", "x86"},
    {"GCC ARM Cross-Compiler", "arm64"},
    {"GDB Debugger (ARM64)",   "arm64"},
    {"ARM64 Kernel Image",     "arm64"},
    {"Universal Utility Package", "all"},
    {"System Log Viewer",       "all"},
    {NULL, NULL}
};

const char *ARCH_OPTIONS[] = {
    "all architectures",
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
parse_sha256sums(const char *path, char ***ret_names)
{
  _cleanup_fclose_ FILE *fp = NULL;
  _cleanup_free_ char *line = NULL;
  size_t linecap = 0;
  ssize_t linelen;
  char **names = NULL;
  int capacity = 0;
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

      if (count >= capacity)
	{
	  capacity = capacity ? capacity * 2 : 16;
	  char **new_names = realloc(names, capacity * sizeof(char *));
	  if (!new_names)
	    {
	      for (int i = 0; i < count; i++)
		free(names[i]);
	      free(names);
	      return -ENOMEM;
	    }
	  names = new_names;
	}

      names[count] = strdup(name);
      if (!names[count])
	{
	  for (int i = 0; i < count; i++)
	    free(names[i]);
	  free(names);
	  return -ENOMEM;
	}
      count++;
    }

  *ret_names = names;

  MSG_INFO("Found %i supported image(s) in '%s'", count, path);

  return count;
}


int get_url_from_list(char **ret)
{
  _cleanup_free_ char *sha256sums_url = NULL;
  _cleanup_free_ char *sha256sums_asc_url = NULL;
  _cleanup_free_ char *sha256sums_fn = NULL;
  _cleanup_free_ char *sha256sums_asc_fn = NULL;
  char **names = NULL;
  int num_names;
  int selected;
  int r = 0;

  MSG_FUNC();

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

  num_names = parse_sha256sums(sha256sums_fn, &names);
  if (num_names < 0)
    {
      MSG_ERROR("Error parsing SHA256SUMS file: %s",
		strerror(-num_names));
      show_error_popup("Error parsing SHA256SUMS file:",
		       strerror(-num_names), NULL);
      return num_names;
    }
  if (num_names == 0)
    {
      MSG_INFO("No supported images found in %s", rdii_download_server);
      show_error_popup("No supported images found in SHA256SUMS file.",
			NULL, NULL);
      return -ENOENT;
    }

  _cleanup_free_ char *header = NULL;
  const char *help_text = "Select raw disk image which has to be installed on the target system.\n"
    "The name of the default download server can be changed by the value of rdii.download_server, set "
    "by the kernel cmdline during boot or by a configuration file.\n";

  if (asprintf(&header, "Select image from download server %s", rdii_download_server) < 0)
    {
      for (int i = 0; i < num_names; i++)
	free(names[i]);
      free(names);
      return -ENOMEM;
    }

  selected = choose_entry(4, (const char **)names, num_names, 0,
                          header, help_text);
  if (selected < 0)
    r = selected;
  else if (asprintf(ret, "%s/%s", rdii_download_server, names[selected]) < 0)
    r = -ENOMEM;
  else
    r = 0;

  for (int i = 0; i < num_names; i++)
    free(names[i]);
  free(names);

  return r;
}
