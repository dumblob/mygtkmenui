/**
 * mygtkmenui - read description file and generate menu
 *
 * Copyright (C) 2004-2011 John Vorthman
 *               2012-2014 Jan Pacner (dumblob@gmail.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License, version 2, as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 59
 * Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * gcc -o mygtkmenui `pkg-config --cflags --libs gtk+-3.0` main.c
 *                                               gtk+-2.0
 */

//FIXME dumblob's long term wishlist
//  -q quiet flag (only errors would be shown)
//  search (case-insensitive + partial token match) using /
//  position submenus such that
//    the middle item is right next to currently selected item in
//      the parent menu
//    OR
//    the first non-submenu item gets selected (if none, select
//      first submenu item)
//  possibility to change font from the menuDescFile?
//  disable selection cycling when entering the first/last item
//  icon upscaling (only downscaling is currently implemented)
//  cache somehow the scaled icons (or defer the reading & scaling up
//    to the first (sub)menu kickoff) for fast menu show-up

#include <stdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <math.h>

#if ! GTK_CHECK_VERSION (2, 18, 0)
#define gtk_widget_get_visible(w) GTK_WIDGET_VISIBLE (w)
#endif

#define MAX_LINE_LENGTH 768
#define MAX_MENU_ENTRIES 1024
#define MAX_SUBMENU_DEPTH 4
#define MAX_ICON_SIZE 256
#define MIN_ICON_SIZE 8
#define MAX_PATH_LEN 256
/* only 1 instance allowed; 0 = No, 1 = Yes, 2 = Error */
#define LOCK_NAME "mygtkmenui_lockfile"
#define ENV_LOCK_FILE_PREFIX "XDG_CACHE_HOME"

#define printHelpMsg(app_name) \
  g_print ("mygtkmenui - myGtkMenu improved\n" \
      "USAGE: %s -h\n" \
      "       %s -- [filename]\n" \
      "  if no `filename' is given, use stdin\n" \
      "  see the attached menu description file for more info\n", \
      (app_name), (app_name));

/* prototypes */
int ReadLine ();
static void RunItem (char *);
static void QuitMenu (char *);
gboolean Get2Numbers (char *);
static void menu_position (GtkMenu *, gint *, gint *, gboolean *, gpointer);
int already_running (void);
void gtk_menu_shell_select_some (GtkMenuShell *);

/* globals */
char Line[MAX_LINE_LENGTH], data[MAX_LINE_LENGTH];
int depth, lineNum, menuX, menuY;
FILE *pFile;
GtkWidget *menu[MAX_SUBMENU_DEPTH];

int main (int argc, char *argv[]) {
  int Mode;		// What kind of input are we looking for?
  int Kind;		// Type of input actually read
  int curDepth;	// Root menu is depth = 0
  int curItem;	// Count number of menu entries
  gint w, h;		// Size of menu icon
  gboolean bSetMenuPos = FALSE;
  int i;
  char Item[MAX_LINE_LENGTH], Cmd[MAX_MENU_ENTRIES][MAX_LINE_LENGTH];
  GtkWidget *menuItem, *SubmenuItem = NULL;
  GtkWidget *image;
  GdkPixbuf *Pixbuf;
  GError *error = NULL;
  struct stat statbuf;

  // Some hot keys (keybindings) get carried away and start
  // many instances. Will try to use a lock file so that
  // only one instance of this program runs at a given time.
  // If this check causes you problems, comment it out.
  int i_already_running = already_running();
  if (i_already_running == 1) {
    fprintf(stderr, "Already running, will quit.\n");
    return EXIT_FAILURE;
  }
  else if (i_already_running == 2) {
    fprintf (stderr, "%s: Error in routine already_running(), will quit.\n",
        argv[0]);
    return EXIT_FAILURE;
  }

  if ((argc == 2 || argc == 3) && ! strcmp(argv[1], "--")) {
    if (argc == 3) {
      printf ("Reading the file: %s\n", argv[2]);
      pFile = fopen (argv[2], "r");
    }
    else {
      printf ("Reading menu-description from stdin...\n");
      pFile = stdin;
    }
  }
  else {
    printHelpMsg(argv[0]);
    return EXIT_SUCCESS;
  }

  if (pFile == NULL) {
    fprintf (stderr, "Cannot open the file.\n");
    return EXIT_FAILURE;
  }

  // manipulates with the content of argc/argv (e.g. -- disappears both
  //   from argc and argv)
  if (!gtk_init_check (&argc, &argv)) {
    g_print("Error, cannot initialize gtk.\n");
    return EXIT_FAILURE;
  }

  menu[0] = gtk_menu_new ();

  /* handle et .c oh un keys as arrow-keys down up left right on dvorak
     keyboard layout and jkqgG keys as in less(1)
     see menu class in gtk/gtkmenu.c from GTK upstream */
  guint key;
  GdkModifierType mod;
  GtkBindingSet *binding_set = gtk_binding_set_by_class (
      GTK_MENU_GET_CLASS (menu[0]));

  gtk_accelerator_parse ("e", &key, &mod);
  gtk_binding_entry_add_signal (binding_set, key, mod,
      "move-current", 1, GTK_TYPE_MENU_DIRECTION_TYPE, GTK_MENU_DIR_NEXT);
  gtk_accelerator_parse ("t", &key, &mod);
  gtk_binding_entry_add_signal (binding_set, key, mod,
      "move-current", 1, GTK_TYPE_MENU_DIRECTION_TYPE, GTK_MENU_DIR_NEXT);
  gtk_accelerator_parse ("j", &key, &mod);
  gtk_binding_entry_add_signal (binding_set, key, mod,
      "move-current", 1, GTK_TYPE_MENU_DIRECTION_TYPE, GTK_MENU_DIR_NEXT);

  //gtk_accelerator_parse (".", &key, &mod);  // why the hell does this not work?
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_period, 0,
      "move-current", 1, GTK_TYPE_MENU_DIRECTION_TYPE, GTK_MENU_DIR_PREV);
  gtk_accelerator_parse ("c", &key, &mod);
  gtk_binding_entry_add_signal (binding_set, key, mod,
      "move-current", 1, GTK_TYPE_MENU_DIRECTION_TYPE, GTK_MENU_DIR_PREV);
  gtk_accelerator_parse ("k", &key, &mod);
  gtk_binding_entry_add_signal (binding_set, key, mod,
      "move-current", 1, GTK_TYPE_MENU_DIRECTION_TYPE, GTK_MENU_DIR_PREV);

  gtk_accelerator_parse ("o", &key, &mod);
  gtk_binding_entry_add_signal (binding_set, key, mod,
      "move-current", 1, GTK_TYPE_MENU_DIRECTION_TYPE, GTK_MENU_DIR_PARENT);
  gtk_accelerator_parse ("h", &key, &mod);
  gtk_binding_entry_add_signal (binding_set, key, mod,
      "move-current", 1, GTK_TYPE_MENU_DIRECTION_TYPE, GTK_MENU_DIR_PARENT);

  gtk_accelerator_parse ("u", &key, &mod);
  gtk_binding_entry_add_signal (binding_set, key, mod,
      "move-current", 1, GTK_TYPE_MENU_DIRECTION_TYPE, GTK_MENU_DIR_CHILD);
  gtk_accelerator_parse ("n", &key, &mod);
  gtk_binding_entry_add_signal (binding_set, key, mod,
      "move-current", 1, GTK_TYPE_MENU_DIRECTION_TYPE, GTK_MENU_DIR_CHILD);
  //gtk_accelerator_parse ("l", &key, &mod);  // why the hell does this not work?
  gtk_binding_entry_add_signal (binding_set, GDK_KEY_l, mod,
      "move-current", 1, GTK_TYPE_MENU_DIRECTION_TYPE, GTK_MENU_DIR_CHILD);

  gtk_accelerator_parse ("g", &key, &mod);
  gtk_binding_entry_add_signal (binding_set, key, mod,
      "move-scroll", 1, GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_START);

  gtk_accelerator_parse ("<Shift>g", &key, &mod);
  gtk_binding_entry_add_signal (binding_set, key, mod,
      "move-scroll", 1, GTK_TYPE_SCROLL_TYPE, GTK_SCROLL_END);

  gtk_accelerator_parse ("q", &key, &mod);
  gtk_binding_entry_add_signal (binding_set, key, mod,
      "cancel", 0);

  if (! gtk_icon_size_lookup (GTK_ICON_SIZE_BUTTON, &w, &h)) {
    w = 30;
    h = 30;
  };

  curItem = 0;
  Mode = 0;
  curDepth = 0;
  while ((Kind = ReadLine()) != 0) {	// Read next line and get the keyword kind
    if (Kind == -1) {
      Mode = -1;	// Error parsing file
      fprintf(stderr, "Bad syntax \"%s\"\n", data);
    }

    if (depth > curDepth) {
      g_print ("Keyword found at incorrect indentation.\n");
      Mode = -1;
    }
    else if (depth < curDepth) {	// Close submenu
      curDepth = depth;
    }

    if (Mode == 0) {	// Starting new sequence. Whats next?
      if (Kind == 1)
        Mode = 1;	// New item
      else if (Kind == 4)
        Mode = 4;	// New submenu
      else if (Kind == 5)
        Mode = 6;	// New separator
      else if (Kind == 6)
        Mode = 7;	// New icon size
      else if (Kind == 7)
        Mode = 8;	// Set menu position
      else {			// Problems
        g_print ("Keyword out of order.\n");
        Mode = -1;
      }
    }

    switch (Mode) {
      case 1:	// Starting new menu item
        if (curItem >= MAX_MENU_ENTRIES) {
          g_print ("Exceeded maximum number of menu items.\n");
          Mode = -1;
          break;
        }
        strcpy (Item, data);
        Mode = 2;
        break;
      case 2:	// Still making new menu item
        if (Kind != 2) {	// Problems if keyword 'cmd=' not found
          g_print ("Missing keyword 'cmd=' (after 'item=').\n");
          Mode = -1;
          break;
        }
        strcpy (Cmd[curItem], data);
        Mode = 3;
        break;
      case 3:	// Still making new menu item
        if (Kind != 3) {	// Problems if keyword 'icon=' not found
          g_print ("Missing keyword 'icon=' (after 'cmd=').\n");
          Mode = -1;
          break;
        }
        // Insert item into menu
        menuItem = gtk_image_menu_item_new_with_mnemonic (Item);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu[curDepth]), menuItem);
        g_signal_connect_swapped (menuItem, "activate",
            G_CALLBACK (RunItem), Cmd[curItem]);
        curItem++;

        #define ADD_MENU_ITEM(img) \
          Mode = 0; \
          if (data[0] == '\0') break; /* no icon */ \
          /* If data is a dir name, program will hang. */ \
          stat (data, &statbuf); \
          if (! S_ISREG(statbuf.st_mode)) { \
            g_print("%d: Error, %s is not a icon file.\n", lineNum, data); \
            break; \
          } \
          Pixbuf = gdk_pixbuf_new_from_file_at_size(data, w, h, &error); \
          if (Pixbuf == NULL) { \
            g_print("%d: %s\n", lineNum, error->message); \
            g_error_free(error); \
            error = NULL; \
          } \
          image = gtk_image_new_from_pixbuf(Pixbuf); \
          gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(img), image);

        ADD_MENU_ITEM(menuItem)
        break;
      case 4:	// Start submenu
        if (curDepth >= MAX_SUBMENU_DEPTH) {
          g_print ("Maximum submenu depth exceeded.\n");
          Mode = -1;
          break;
        }
        SubmenuItem = gtk_image_menu_item_new_with_mnemonic (data);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu[curDepth]), SubmenuItem);
        curDepth++;
        menu[curDepth] = gtk_menu_new ();
        gtk_menu_item_set_submenu (GTK_MENU_ITEM (SubmenuItem),
            menu[curDepth]);
        Mode = 5;
        break;
      case 5:	// Adding image to new submenu
        if (Kind != 3) {	// Problems if keyword 'icon=' not found
          g_print ("Missing keyword 'icon=' (after 'submenu=').\n");
          Mode = -1;
          break;
        }
        ADD_MENU_ITEM(SubmenuItem)
        break;
      case 6:	// Insert separator into menu
        menuItem = gtk_separator_menu_item_new ();
        gtk_menu_shell_append (GTK_MENU_SHELL (menu[curDepth]), menuItem);
        Mode = 0;
        break;
      case 7:	// Change menu icon size
        i = atoi (data);
        if ((i < MIN_ICON_SIZE) || (i > MAX_ICON_SIZE)) {
          g_print ("Illegal size for menu icon.\n");
          Mode = -1;
          break;
        }
        w = i;
        h = i;
        g_print ("New icon size = %d.\n", w);
        Mode = 0;
        break;
      case 8:	// Set menu position
        if (Get2Numbers (data)) {
          bSetMenuPos = TRUE;
          //g_print ("Menu position = %d, %d.\n", menuX, menuY);
          Mode = 0;
        }
        else {
          g_print ("Error reading menu position.\n");
          Mode = -1;
        }
        break;
      default:
        Mode = -1;
    }	// switch

    if (Mode == -1) {	// Error parsing file
      // Placed here so that ReadLine is not called again
      g_print ("%d: >>>>%s\n", lineNum, Line);
      break;	// Quit reading file
    }
  }	// while

  fclose (pFile);

  // keep user informed, the menu is empty
  if (curItem == 0)
    gtk_menu_shell_append (GTK_MENU_SHELL (menu[0]),
        gtk_image_menu_item_new_with_mnemonic ("<no content to display>"));

  gtk_widget_show_all (menu[0]);
  g_signal_connect_swapped (menu[0], "deactivate",
      G_CALLBACK (QuitMenu), NULL);
  gtk_menu_shell_select_some (GTK_MENU_SHELL (menu[0]));

  while(!gtk_widget_get_visible(menu[0])) { // Keep trying until startup
    usleep(50000);						  // button (or key) is released
    if (bSetMenuPos)
      gtk_menu_popup (GTK_MENU (menu[0]), NULL, NULL,
          (GtkMenuPositionFunc) menu_position,
          NULL, 0, gtk_get_current_event_time ());
    else
      gtk_menu_popup (GTK_MENU (menu[0]), NULL, NULL, NULL, NULL, 0,
          gtk_get_current_event_time ());
    gtk_main_iteration();
  }

  gtk_main ();
  return 0;
}

#if (GTK_MAJOR_VERSION == 2)
#define CHILDREN menu_shell->children
#else
/* first item of menu_shell->priv->children is the needed pointer */
#define CHILDREN ((struct { GList *x; } *)menu_shell->priv)->x
#endif

/* select some item (in this case the middle one) */
void gtk_menu_shell_select_some (GtkMenuShell *menu_shell) {
  GList *tmp_list = CHILDREN;
  guint i = 0;

  /* count all */
  while (tmp_list) {
    i++;
    tmp_list = tmp_list->next;
  }

  guint middle = trunc(i / 2);
  i = 0;
  tmp_list = CHILDREN;

  while (tmp_list) {
    if (i++ == middle) {
      gtk_menu_shell_select_item (menu_shell, GTK_WIDGET(tmp_list->data));
      return;
    }

    tmp_list = tmp_list->next;
  }
}

void menu_position (GtkMenu *menu, gint *x, gint *y, gboolean *push_in,
    gpointer data) {
  /* handle given position as the center of this window (menu) */
  GtkRequisition req;
#if (GTK_MAJOR_VERSION == 2)
  gtk_widget_size_request(GTK_WIDGET(menu), &req);
#else
  gtk_widget_get_preferred_size(GTK_WIDGET(menu), &req, NULL);
#endif
  *x = menuX - (req.width >> 1);
  *y = menuY - (req.height >> 1);
  *push_in = TRUE;
}

gboolean Get2Numbers (char *data) {
  int n, i;

  n = strlen (data);
  if ((n == 0) | !isdigit (data[0]))
    return FALSE;
  menuX = atoi (data);
  i = 0;

  // Skip over first number
  while (isdigit (data[i])) {
    i++;
    if (i == n)
      return FALSE;
  }

  // Find start of the next number
  while (! isdigit (data[i])) {
    i++;
    if (i == n)	return FALSE;
  }

  menuY = atoi (&data[i]);
  return TRUE;
}

static void RunItem (char *Cmd) {
  GError *error = NULL;
  //g_print ("Run: %s\n", Cmd);

  if (!Cmd) return;

  if (!g_spawn_command_line_async(Cmd, &error)) {
    g_print ("Error running command.\n");
    g_print ("%s\n", error->message);
    g_error_free (error);
  }
  gtk_main_quit ();
}

static void QuitMenu (char *Msg) {
  //g_print ("Menu was deactivated.\n");
  gtk_main_quit ();
}

// return rest of the string after cut off using regexp |^pattern *= *|
char *str_starts_with_pattern (char *str, char *pattern) {
  for (;;) {
    if (*str == '\0') return NULL;

    // pattern matches
    if (*pattern == '\0') {
      for (; *str == ' '; ++str);

      if (*str != '=') return NULL;

      for (++str; *str == ' '; ++str);

      // empty string (containing only '\0') allowed
      return str;
    }
    // in the middle of pattern
    else if (*str == *pattern) {
      ++str;
      ++pattern;
    }
    else {
      return NULL;
    }
  }
}

/* return kind of line, menu depth, and stripped text */
int ReadLine () {
  // return(-1) = Error, return(0) = EOF, return(>0) = keyword
  char *chop;
  int i, len, Kind;
  char tmp[MAX_LINE_LENGTH];
  char *str2;

  len = 0;
  while (len == 0) {
    // Read one line.
    if (fgets (Line, MAX_LINE_LENGTH, pFile) == NULL)
      return (0);
    strcpy (tmp, Line);
    lineNum++;

    // Remove comments
    chop = strchr (tmp, '#');
    if (chop != 0)
      *chop = '\0';

    len = strlen (tmp);

    // Remove trailing spaces & CR/LF
    if (len > 0) {
      chop = &tmp[len - 1];
      while ((chop >= tmp) && (isspace(*chop) != 0)) {
        *chop = '\0';
        chop--;
      }
      len = strlen(tmp);
    }
  };

  // Big error?
  if (len >= MAX_LINE_LENGTH) {
    strncpy (data, tmp, MAX_LINE_LENGTH);
    data[MAX_LINE_LENGTH - 1] = '\0';
    return (-1);
  }

  // Calculate menu depth
  for (depth = 0, i = 0; i < len; i++) {
    if (tmp[i] == '\t')
      depth++;
    else
      break;
  }

  // Remove leading white space
  str2 = tmp;
  while ((*str2 == ' ') || (*str2 == '\t')) {
    str2++;
    len--;
  }
  for (i = 0; i <= len; i++) tmp[i] = str2[i];

  if (! strcmp(tmp, "separator")) {
    str2 = tmp;
    Kind = 5;
  }
  else if ((str2 = str_starts_with_pattern (tmp, "cmd"     )) != NULL) {
    Kind = 2;
  }
  else if ((str2 = str_starts_with_pattern (tmp, "item"    )) != NULL) {
    Kind = 1;
  }
  else if ((str2 = str_starts_with_pattern (tmp, "icon"    )) != NULL) {
    Kind = 3;
  }
  else if ((str2 = str_starts_with_pattern (tmp, "submenu" )) != NULL) {
    Kind = 4;
  }
  else if ((str2 = str_starts_with_pattern (tmp, "menupos" )) != NULL) {
    Kind = 7;
  }
  else if ((str2 = str_starts_with_pattern (tmp, "iconsize")) != NULL) {
    Kind = 6;
  }
  /* its a bad line */
  else {
    str2 = tmp;
    Kind = -1;
  }

  strcpy (data, str2);
  return Kind;
}

int already_running (void) {
  struct flock fl;
  int fd;
  char *home;
  int n;
  int ret = 2;
  char *Lock_path;

  /* a buffer to hold the path name to our lock file */
  if ((Lock_path = malloc(MAX_PATH_LEN + 1)) == NULL) {
    fprintf(stderr,
        "Error, malloc failed");
    exit(1);
  }

  /* we need to place the lock file in ENV_LOCK_FILE_PREFIX if it is set,
     else in ~/.cache */
  if ((home = getenv(ENV_LOCK_FILE_PREFIX)) != NULL) {
    n = snprintf(Lock_path, MAX_PATH_LEN, "%s/%s", home, LOCK_NAME);
  } else if ((home = getenv("HOME")) != NULL) {
    /* try for ~/.cache instead */
    n = snprintf(Lock_path, MAX_PATH_LEN, "%s/.cache/%s", home, LOCK_NAME);
  } else {
    fprintf(stderr,
        "Error, could not get env variables " ENV_LOCK_FILE_PREFIX " or HOME\n");
    goto Done;
  }

  if (n > MAX_PATH_LEN || Lock_path == NULL) {
    fprintf(stderr, "Error, path name too long: %s.\n", Lock_path);
    goto Done;
  }

  fd = open(Lock_path, O_RDWR | O_CREAT, 0600);
  if (fd < 0) {
    fprintf(stderr, "Error opening %s.\n", Lock_path);
    goto Done;
  }

  fl.l_start = 0;
  fl.l_len = 0;
  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;

  if (fcntl(fd, F_SETLK, &fl) < 0)
    ret = 1;
  else
    ret = 0;

Done:
  free(Lock_path);

  return ret;
}
