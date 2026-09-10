// SPDX-License-Identifier: GPL-2.0-or-later

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <locale.h>
#include <wchar.h>
#include <ctype.h>

#include "basics.h"
#include "logger.h"
#include "nc-dialogs.h"

static const char *header_title = NULL;

static void
init_colors(void)
{
  MSG_FUNC();

  start_color();
  use_default_colors();

  init_pair(CP_HEADER, COLOR_GREEN, COLOR_BLUE);
  init_pair(CP_SPLASH_BOX, COLOR_GREEN, COLOR_BLUE);
  init_pair(CP_WARNING, COLOR_WHITE, COLOR_RED);
  init_pair(CP_SELECTED, COLOR_GREEN, -1);

  if (COLORS >= 256)
    {
      init_pair(CP_TITLE, 21, -1);      // Blue color 21
      init_pair(CP_UNSELECTED, 8, -1);  // Gray color 8
    }
  else
    {
      init_pair(CP_TITLE, COLOR_BLUE, -1);
      init_pair(CP_UNSELECTED, COLOR_WHITE, -1);
    }

  // Footer: Default font
  init_pair(CP_FOOTER, -1, -1);
}

void
print_global_header_footer(const char *addkeys, const bool selection)
{
  MSG_FUNC("addkeys='%s'", strempty(addkeys));

  clear();
  // Draw Header (Green on Blue)
  attron(COLOR_PAIR(CP_HEADER) | A_BOLD);
  mvhline(0, 0, ' ', COLS); // Fill the top line background
  mvprintw(0, (COLS - strlen(strempty(header_title))) / 2, "%s", strempty(header_title));
  attroff(COLOR_PAIR(CP_HEADER) | A_BOLD);

  // Draw Footer
  const char *footer_text = (selection ? "Up/Down: Navigate | Enter: Select | ESC: Abort/Quit" :
                             "ESC: Abort/Quit");
  attron(COLOR_PAIR(CP_FOOTER) | A_REVERSE);
  mvhline(LINES - 1, 0, ' ', COLS);
  if (addkeys)
    mvprintw(LINES - 1, 1, "%s | %s", addkeys, footer_text);
  else
    mvprintw(LINES - 1, 1, "%s", footer_text);
  attroff(COLOR_PAIR(CP_FOOTER) | A_REVERSE);
}

void
print_title(const char *title)
{
  MSG_FUNC("title='%s'", strna(title));

  attron(COLOR_PAIR(CP_TITLE));
  mvprintw(2, 2, "%s", title);
  attroff(COLOR_PAIR(CP_TITLE));
}


static char*
truncate_middle(const char *str, size_t max_len)
{
  size_t len = strlen(str);

  if (len <= max_len)
    return strdup(str); // Yes, we ignore OOM...

  char *result = malloc(max_len + 1);
  if (result == NULL)
    return NULL;

  // If max_len is too small to even fit the "...", fallback to a hard truncation at the end
  if (max_len < 3)
    {
      strncpy(result, str, max_len);
      result[max_len] = '\0';
      return result;
    }

  // Calculate how many characters from the original string we can keep
  size_t keep_len = max_len - 3;
  size_t prefix_len = keep_len / 2;
  size_t suffix_len = keep_len - prefix_len;

  // Construct the new string: [prefix] + "..." + [suffix]
  strncpy(result, str, prefix_len);
  char *cp = stpcpy(result + prefix_len, "...");
  stpcpy(cp, str + (len - suffix_len));

  return result;
}


// Returns 1 if YES, 0 if NO
int
show_warning_popup(const char *headline,
		   const char *descr_line1, const char *descr_line2)
{
  unsigned int height = 7;
  unsigned int width;

  MSG_FUNC("headline='%s', descr1='%s', descr2='%s'", strna(headline),
	   strna(descr_line1), strna(descr_line2));

  if (descr_line1)
    height++;

  if (descr_line2)
    height++;

  width = strlen(headline) + 6;
  if (descr_line1)
    {
      if (strlen(descr_line1) > (size_t)(COLS - 8))
	descr_line1 = truncate_middle(descr_line1, COLS-8);
      if (strlen(descr_line1) + 6 > width)
	width = strlen(descr_line1) + 6;
    }
  if (descr_line2)
    {
      if (strlen(descr_line2) > (size_t)(COLS - 8))
	descr_line2 = truncate_middle(descr_line2, COLS-8);
      if (strlen(descr_line2) + 6 > width)
	width = strlen(descr_line2) + 6;
    }

  int start_y = (LINES - height) / 2 - 2;
  int start_x = (COLS - width) / 2;

  WINDOW *win = newwin(height, width, start_y, start_x);
  wbkgd(win, COLOR_PAIR(CP_WARNING));
  keypad(win, TRUE); // Enable arrow keys for this specific window

  int btn_selected = 1; // 0 = YES, 1 = NO (Defaulting to NO for safety)
  int choice = -1;

  while (1)
    {
      box(win, 0, 0);
      mvwprintw(win, 2, (width - strlen(headline)) / 2, "%s", headline);
      if (descr_line1)
	mvwprintw(win, 4, (width - strlen(descr_line1)) / 2, "%s", descr_line1);
      if (descr_line2)
	mvwprintw(win, 5, (width - strlen(descr_line2)) / 2, "%s", descr_line2);

      if (btn_selected == 0)
	{
	  wattron(win, A_REVERSE);
	  mvwprintw(win, height - 2, width / 2 - 10, "[ YES ]");
	  wattroff(win, A_REVERSE);
        }
      else
	mvwprintw(win, height - 2, width / 2 - 10, "[ YES ]");

      if (btn_selected == 1)
	{
	  wattron(win, A_REVERSE);
	  mvwprintw(win, height - 2, width / 2 + 3, "[ NO ]");
	  wattroff(win, A_REVERSE);
        }
      else
	mvwprintw(win, height - 2, width / 2 + 3, "[ NO ]");

      wrefresh(win);

      // Handle input locally inside the popup
      int key = wgetch(win);
      if (key == KEY_LEFT || key == KEY_RIGHT || key == '\t')
	  btn_selected = 1 - btn_selected; // Toogle
      else if (key == '\n' || key == KEY_ENTER)
	{
	  choice = btn_selected;
	  break;
        }
      else if (key == 27) // ESC key aborts/defaults to NO
	{
	  choice = 1;
	  break;
        }
    }

    delwin(win);
    refresh();

    return (choice == 0); // Return 1 if YES was chosen, otherwise 0
}

void
show_error_popup(const char *headline,
		 const char *descr_line1, const char *descr_line2)
{
  int height = 7;
  int width = strlen(headline) + 6;

  MSG_FUNC("headline='%s', descr1='%s', descr2='%s'", strna(headline),
	   strna(descr_line1), strna(descr_line2));

  if (headline)
    height++;

  if (descr_line1)
    {
      height++;
      if ((int)(strlen(descr_line1) + 6) > width)
	width = strlen(descr_line1) + 6;
    }
  if (descr_line2)
    {
      height++;
      if ((int)(strlen(descr_line2) + 6) > width)
	width = strlen(descr_line2) + 6;
    }

  int start_y = (LINES - height) / 2 - 2;
  int start_x = (COLS - width) / 2;

  WINDOW *win = newwin(height, width, start_y, start_x);
  wbkgd(win, COLOR_PAIR(CP_WARNING));
  box(win, 0, 0);
  mvwprintw(win, 2, (width - strlen(headline)) / 2, "%s", headline);
  if (descr_line1)
    mvwprintw(win, 4, (width - strlen(descr_line1)) / 2, "%s", descr_line1);
  if (descr_line2)
    mvwprintw(win, 5, (width - strlen(descr_line2)) / 2, "%s", descr_line2);

  mvwprintw(win, height - 3, width / 2 - 3, "[ OK ]");
  wrefresh(win);

  while (1)
    {
      // Handle input locally inside the popup
      int key = wgetch(win);
      if (key == '\n' || key == 27) // RETURN || ESC
	break;
    }

  delwin(win);
}

void
show_info_popup(const char *headline, const char *descr)
{
  int height = 6;
  int width = strlen(headline) + 6;

  MSG_FUNC("headline='%s', descr='%s'", headline, strna(descr));

  if (descr)
    {
      height += 2;
      if ((int)(strlen(descr) + 6) > width)
	width = strlen(descr) + 6;
    }

  int start_y = (LINES - height) / 2 - 2;
  int start_x = (COLS - width) / 2;

  WINDOW *win = newwin(height, width, start_y, start_x);
  wbkgd(win, COLOR_PAIR(CP_SPLASH_BOX));
  keypad(win, TRUE);
  box(win, 0, 0);
  mvwprintw(win, 2, (width - strlen(headline)) / 2, "%s", headline);
  if (descr)
    mvwprintw(win, 4, (width - strlen(descr)) / 2, "%s", descr);
  mvwprintw(win, height - 2, width / 2 - 3, "[ OK ]");

  // Use wgetch/wrefresh on the popup window instead of keywait() which
  // calls refresh() on stdscr.  After clear(), stdscr has clearok set,
  // so a refresh() would flush blank stdscr cells over the popup window
  // in the virtual screen, making the popup invisible.
  wtimeout(win, 100);

  const char spinner[] = "|/-\\";
  int spinner_idx = 0;
  int elapsed_ms = 0;

  while (elapsed_ms < 30 * 1000)
    {
      mvwprintw(win, height - 2, width - 2, "%c", spinner[spinner_idx]);
      spinner_idx = (spinner_idx + 1) % 4;
      wrefresh(win);
      if (wgetch(win) != ERR)
	break;
      elapsed_ms += 100;
    }

  delwin(win);
  refresh();
}

void show_help_dialog(const char *title, const char *text) {
  int max_y, max_x;
  getmaxyx(stdscr, max_y, max_x);

  if (!text)
    return;

  // Set dialog dimensions (e.g., 60% of terminal screen)
  int height = max_y * 0.6;
  int width = max_x * 0.6;

  // Fallback bounds for small terminals
  if (height < 8) height = 8;
  if (width < 30) width = 30;

  int start_y = (max_y - height) / 2;
  int start_x = (max_x - width) / 2;

  int prev_cursor = curs_set(0);

  // Create main window and internal text pad area
  WINDOW *help_win = newwin(height, width, start_y, start_x);
  int text_win_h = height - 4;
  int text_win_w = width - 4;
  WINDOW *text_win = derwin(help_win, text_win_h, text_win_w, 2, 2);

  keypad(help_win, TRUE);

  // --- Line Wrapping Logic ---
  // Break the input string into lines bounded by text_win_w
  int max_lines = 1024; // Allocate buffer for wrapped lines
  char **lines = malloc(sizeof(char *) * max_lines);
  int line_count = 0;

  const char *ptr = text;
  while (*ptr && line_count < max_lines) {
    if (*ptr == '\n')
      {
        lines[line_count] = strdup("");
        line_count++;
        ptr++;
        continue;
      }

    // Measure line length up to newline or maximum width
    int len = 0;
    int break_point = -1;

    while (ptr[len] && ptr[len] != '\n' && len < text_win_w)
      {
        if (isspace((unsigned char)ptr[len]))
          {
            break_point = len;
          }
        len++;
      }

    // Handle word wrap: break at space if line exceeds width
    if (len == text_win_w && ptr[len] != '\0' && ptr[len] != '\n' && break_point > 0)
      {
        len = break_point;
      }

    char *line_buf = malloc(len + 1);
    strncpy(line_buf, ptr, len);
    line_buf[len] = '\0';
    lines[line_count++] = line_buf;

    ptr += len;
    if (*ptr == ' ' || *ptr == '\n')
      {
        ptr++; // Skip whitespace delimiter
      }
  }

  int scroll_offset = 0;
  int ch;

  // --- Main Event Loop ---
  while (1)
    {
      // Redraw outer dialog frame
      werase(help_win);
      box(help_win, 0, 0);

      if (title)
        {
          mvwprintw(help_win, 0, (width - (int)strlen(title) - 2) / 2, " %s ", title);
        }

      const char *footer = (line_count > text_win_h) ? " Up/Down: Scroll | Press any key to exit " :
        "  Press any key to exit ";
      mvwprintw(help_win, height - 1, (width - (int)strlen(footer)) / 2, "%s", footer);

      // Render visible slice of text
      werase(text_win);
      for (int i = 0; i < text_win_h; i++)
        {
          int current_line = scroll_offset + i;
          if (current_line < line_count)
            {
              mvwprintw(text_win, i, 0, "%s", lines[current_line]);
            }
        }

      // Draw scroll bar if text overflows window height
      if (line_count > text_win_h)
        {
          int track_height = text_win_h;
          int bar_size = (track_height * text_win_h) / line_count;
          if (bar_size < 1) bar_size = 1;

          int max_offset = line_count - text_win_h;
          int bar_pos = (scroll_offset * (track_height - bar_size)) / max_offset;

          // Draw track along the right edge
          for (int y = 0; y < track_height; y++)
            {
              mvwaddch(help_win, 2 + y, width - 1, ACS_VLINE);
            }

          // Draw scroll thumb/handle
          wattron(help_win, A_REVERSE);
          for (int y = 0; y < bar_size; y++)
            {
              mvwaddch(help_win, 2 + bar_pos + y, width - 1, ' ');
            }
          wattroff(help_win, A_REVERSE);
        }

      wrefresh(help_win);
      wrefresh(text_win);

      // Navigation input processing
      ch = wgetch(help_win);
      if (ch == KEY_UP)
        {
          if (scroll_offset > 0) scroll_offset--;
        }
      else if (ch == KEY_DOWN)
        {
          if (scroll_offset < line_count - text_win_h) scroll_offset++;
        }
      else if (ch == KEY_NPAGE)
        { // Page Down
          scroll_offset += text_win_h;
          if (scroll_offset > line_count - text_win_h)
            scroll_offset = line_count - text_win_h;
          if (scroll_offset < 0) scroll_offset = 0;
        }
      else if (ch == KEY_PPAGE)
        { // Page Up
          scroll_offset -= text_win_h;
          if (scroll_offset < 0) scroll_offset = 0;
        }
      else
        {
          // Exit loop on any other key press
          break;
        }
    }

  // Cleanup allocated memory
  for (int i = 0; i < line_count; i++)
    {
      free(lines[i]);
    }
  free(lines);
  // Cleanup ncurses resources
  delwin(text_win);
  delwin(help_win);

  curs_set(prev_cursor);
  touchwin(stdscr);
  refresh();
}

int
choose_entry(int row, const char *options[], int num_options, int start,
             const char *title, const char *help_text)
{
  int selected = start;

  MSG_FUNC("row=%i, options[0]='%s', num_options=%i, start=%i, title=%s",
           row, options[0], num_options, start, title);

  print_global_header_footer((help_text ? "F1: Help" : NULL),
                             SELECTION);
  print_title((title ? title : ""));

  while (1)
    {
      for (int i = 0; i < num_options; i++)
	{
	  int y = row + i;

	  if (i == selected)
	    {
	      attron(COLOR_PAIR(CP_SELECTED) | A_BOLD);
	      mvprintw(y, 2, "-> %s", options[i]);
	      attroff(COLOR_PAIR(CP_SELECTED) | A_BOLD);
	    }
	  else
	    {
	      attron(COLOR_PAIR(CP_UNSELECTED));
	      mvprintw(y, 2, "   %s", options[i]);
	      attroff(COLOR_PAIR(CP_UNSELECTED));
	    }
	}

      refresh();

      int ch = getch();
      if (ch == 27) // 27 is the ASCII code for ESC
	{
	  MSG_INFO("Canceld with ESC");
	  return -ECANCELED;
	}
      else if (ch == KEY_UP)
	selected = (selected - 1 + num_options) % num_options;
      else if (ch == KEY_DOWN)
	selected = (selected + 1) % num_options;
      else if (ch == KEY_F1)
        show_help_dialog(title, help_text);
      else if (ch == '\n' || ch == KEY_ENTER)
	{
	  MSG_INFO("Selected entry %i", selected);
	  return selected;
	}
    }

  MSG_ERROR("quit while loop without return!");

  // we should never reach this
  return -2;
}

void
init_ncurses(const char *title)
{
  // For correctly rendering the double borders
  setlocale(LC_ALL, "");

  MSG_FUNC("title='%s'", strna(title));

  header_title = title;

  // Initialize ncurses
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE); // Enable arrow keys
  curs_set(0);          // Hide cursor
  set_escdelay(25);     // Set escape delay to 25 milliseconds

  if (has_colors())
    init_colors();
}
