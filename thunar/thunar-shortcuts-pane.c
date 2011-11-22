/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2011      Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-shortcuts-pane.h>
#include <thunar/thunar-shortcuts-pane-ui.h>
#include <thunar/thunar-shortcuts-view.h>
#include <thunar/thunar-side-pane.h>
#include <thunar/thunar-stock.h>



enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_SELECTED_FILES,
  PROP_SHOW_HIDDEN,
  PROP_UI_MANAGER,
};



static void          thunar_shortcuts_pane_component_init        (ThunarComponentIface     *iface);
static void          thunar_shortcuts_pane_navigator_init        (ThunarNavigatorIface     *iface);
static void          thunar_shortcuts_pane_side_pane_init        (ThunarSidePaneIface      *iface);
static void          thunar_shortcuts_pane_dispose               (GObject                  *object);
static void          thunar_shortcuts_pane_finalize              (GObject                  *object);
static void          thunar_shortcuts_pane_get_property          (GObject                  *object,
                                                                  guint                     prop_id,
                                                                  GValue                   *value,
                                                                  GParamSpec               *pspec);
static void          thunar_shortcuts_pane_set_property          (GObject                  *object,
                                                                  guint                     prop_id,
                                                                  const GValue             *value,
                                                                  GParamSpec               *pspec);
static ThunarFile   *thunar_shortcuts_pane_get_current_directory (ThunarNavigator          *navigator);
static void          thunar_shortcuts_pane_set_current_directory (ThunarNavigator          *navigator,
                                                                  ThunarFile               *current_directory);
static GList        *thunar_shortcuts_pane_get_selected_files    (ThunarComponent          *component);
static void          thunar_shortcuts_pane_set_selected_files    (ThunarComponent          *component,
                                                                  GList                    *selected_files);
static GtkUIManager *thunar_shortcuts_pane_get_ui_manager        (ThunarComponent          *component);
static void          thunar_shortcuts_pane_set_ui_manager        (ThunarComponent          *component,
                                                                  GtkUIManager             *ui_manager);
static void          thunar_shortcuts_pane_action_shortcuts_add  (GtkAction                *action,
                                                                  ThunarShortcutsPane      *shortcuts_pane);



struct _ThunarShortcutsPaneClass
{
  GtkScrolledWindowClass __parent__;
};

struct _ThunarShortcutsPane
{
  GtkScrolledWindow __parent__;

  ThunarFile       *current_directory;
  GList            *selected_files;

  GtkActionGroup   *action_group;
  GtkUIManager     *ui_manager;
  guint             ui_merge_id;

  GtkWidget        *view;
};



static const GtkActionEntry action_entries[] =
{
  { "sendto-shortcuts", THUNAR_STOCK_SHORTCUTS, "", NULL, NULL,
    G_CALLBACK (thunar_shortcuts_pane_action_shortcuts_add), },
};



G_DEFINE_TYPE_WITH_CODE (ThunarShortcutsPane,
                         thunar_shortcuts_pane, 
                         GTK_TYPE_SCROLLED_WINDOW,
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_NAVIGATOR,
                                                thunar_shortcuts_pane_navigator_init)
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_COMPONENT,
                                                thunar_shortcuts_pane_component_init)
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_SIDE_PANE,
                                                thunar_shortcuts_pane_side_pane_init))



static void
thunar_shortcuts_pane_class_init (ThunarShortcutsPaneClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_shortcuts_pane_dispose;
  gobject_class->finalize = thunar_shortcuts_pane_finalize;
  gobject_class->get_property = thunar_shortcuts_pane_get_property;
  gobject_class->set_property = thunar_shortcuts_pane_set_property;

  /* override ThunarNavigator's properties */
  g_object_class_override_property (gobject_class,
                                    PROP_CURRENT_DIRECTORY, "current-directory");

  /* override ThunarComponent's properties */
  g_object_class_override_property (gobject_class,
                                    PROP_SELECTED_FILES, "selected-files");
  g_object_class_override_property (gobject_class,
                                    PROP_UI_MANAGER, "ui-manager");

  /* override ThunarSidePane's properties */
  g_object_class_override_property (gobject_class, PROP_SHOW_HIDDEN, "show-hidden");
}



static void
thunar_shortcuts_pane_component_init (ThunarComponentIface *iface)
{
  iface->get_selected_files = thunar_shortcuts_pane_get_selected_files;
  iface->set_selected_files = thunar_shortcuts_pane_set_selected_files;
  iface->get_ui_manager = thunar_shortcuts_pane_get_ui_manager;
  iface->set_ui_manager = thunar_shortcuts_pane_set_ui_manager;
}



static void
thunar_shortcuts_pane_navigator_init (ThunarNavigatorIface *iface)
{
  iface->get_current_directory = thunar_shortcuts_pane_get_current_directory;
  iface->set_current_directory = thunar_shortcuts_pane_set_current_directory;
}



static void
thunar_shortcuts_pane_side_pane_init (ThunarSidePaneIface *iface)
{
  iface->get_show_hidden = (gpointer) exo_noop_false;
  iface->set_show_hidden = (gpointer) exo_noop;
}



static void
thunar_shortcuts_pane_init (ThunarShortcutsPane *shortcuts_pane)
{
  GtkWidget *viewport;

  /* setup the action group for the shortcuts actions */
  shortcuts_pane->action_group = gtk_action_group_new ("ThunarShortcutsPane");
  gtk_action_group_set_translation_domain (shortcuts_pane->action_group,
                                           GETTEXT_PACKAGE);
  gtk_action_group_add_actions (shortcuts_pane->action_group, action_entries,
                                G_N_ELEMENTS (action_entries), shortcuts_pane);

  /* configure the GtkScrolledWindow */
  gtk_scrolled_window_set_hadjustment (GTK_SCROLLED_WINDOW (shortcuts_pane), NULL);
  gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (shortcuts_pane), NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (shortcuts_pane),
                                       GTK_SHADOW_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (shortcuts_pane),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  /* create a viewport for the shortcuts view */
  viewport = gtk_viewport_new (NULL, NULL);
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (viewport), GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (shortcuts_pane), viewport);
  gtk_widget_show (viewport);

  /* allocate the shortcuts view */
  shortcuts_pane->view = thunar_shortcuts_view_new ();
  gtk_container_add (GTK_CONTAINER (viewport), shortcuts_pane->view);
  gtk_widget_show (shortcuts_pane->view);

  /* connect the "shortcut-activated" signal */
  g_signal_connect_swapped (G_OBJECT (shortcuts_pane->view), "shortcut-activated",
                            G_CALLBACK (thunar_navigator_change_directory),
                            shortcuts_pane);
}



static void
thunar_shortcuts_pane_dispose (GObject *object)
{
  ThunarShortcutsPane *shortcuts_pane = THUNAR_SHORTCUTS_PANE (object);

  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (shortcuts_pane), NULL);
  thunar_component_set_selected_files (THUNAR_COMPONENT (shortcuts_pane), NULL);
  thunar_component_set_ui_manager (THUNAR_COMPONENT (shortcuts_pane), NULL);

  (*G_OBJECT_CLASS (thunar_shortcuts_pane_parent_class)->dispose) (object);
}



static void
thunar_shortcuts_pane_finalize (GObject *object)
{
  ThunarShortcutsPane *shortcuts_pane = THUNAR_SHORTCUTS_PANE (object);

  /* release our action group */
  g_object_unref (G_OBJECT (shortcuts_pane->action_group));

  (*G_OBJECT_CLASS (thunar_shortcuts_pane_parent_class)->finalize) (object);
}



static void
thunar_shortcuts_pane_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GtkUIManager *ui_manager;
  ThunarFile   *directory;
  gboolean      show_hidden;
  GList        *selected_files;

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object));
      g_value_set_object (value, directory);
      break;

    case PROP_SELECTED_FILES:
      selected_files = thunar_component_get_selected_files (THUNAR_COMPONENT (object));
      g_value_set_boxed (value, selected_files);
      break;

    case PROP_SHOW_HIDDEN:
      show_hidden = thunar_side_pane_get_show_hidden (THUNAR_SIDE_PANE (object));
      g_value_set_boolean (value, show_hidden);
      break;

    case PROP_UI_MANAGER:
      ui_manager = thunar_component_get_ui_manager (THUNAR_COMPONENT (object));
      g_value_set_object (value, ui_manager);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_shortcuts_pane_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (object),
                                              g_value_get_object (value));
      break;

    case PROP_SELECTED_FILES:
      thunar_component_set_selected_files (THUNAR_COMPONENT (object),
                                           g_value_get_boxed (value));
      break;

    case PROP_SHOW_HIDDEN:
      thunar_side_pane_set_show_hidden (THUNAR_SIDE_PANE (object),
                                        g_value_get_boolean (value));
      break;

    case PROP_UI_MANAGER:
      thunar_component_set_ui_manager (THUNAR_COMPONENT (object),
                                       g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static ThunarFile*
thunar_shortcuts_pane_get_current_directory (ThunarNavigator *navigator)
{
  return THUNAR_SHORTCUTS_PANE (navigator)->current_directory;
}



static void
thunar_shortcuts_pane_set_current_directory (ThunarNavigator *navigator,
                                             ThunarFile      *current_directory)
{
  ThunarShortcutsPane *shortcuts_pane = THUNAR_SHORTCUTS_PANE (navigator);

  /* disconnect from the previously set current directory */
  if (G_LIKELY (shortcuts_pane->current_directory != NULL))
    g_object_unref (G_OBJECT (shortcuts_pane->current_directory));

  /* activate the new directory */
  shortcuts_pane->current_directory = current_directory;

  /* connect to the new directory */
  if (G_LIKELY (current_directory != NULL))
    {
      /* take a reference on the new directory */
      g_object_ref (G_OBJECT (current_directory));

      /* select the file in the view (if possible) */
      thunar_shortcuts_view_select_by_file (THUNAR_SHORTCUTS_VIEW (shortcuts_pane->view),
                                            current_directory);
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (shortcuts_pane), "current-directory");
}



static GList*
thunar_shortcuts_pane_get_selected_files (ThunarComponent *component)
{
  return THUNAR_SHORTCUTS_PANE (component)->selected_files;
}



static void
thunar_shortcuts_pane_set_selected_files (ThunarComponent *component,
                                          GList           *selected_files)
{
  ThunarShortcutsPane *shortcuts_pane = THUNAR_SHORTCUTS_PANE (component);
  GtkAction           *action;
  gboolean             show_action = FALSE;
  GList               *lp;
  gint                 n;

  /* disconnect from the previously selected files... */
  thunar_file_list_free (shortcuts_pane->selected_files);

  /* ...and take a copy of the newly selected files */
  shortcuts_pane->selected_files = thunar_file_list_copy (selected_files);

  /* check if the selection contains only folders */
  for (lp = selected_files, n = 0; lp != NULL; lp = lp->next, ++n)
    if (!thunar_file_is_directory (lp->data))
      break;

  /* change the visibility of the "shortcuts-add" action appropriately */
  action = gtk_action_group_get_action (shortcuts_pane->action_group,
                                        "sendto-shortcuts");
  if (lp == NULL && selected_files != NULL)
    {
      /* check if at least one of the files is not a bookmark yet */
      for (lp = selected_files; !show_action && lp != NULL; lp = lp->next)
        {
          if (!thunar_shortcuts_view_has_file (THUNAR_SHORTCUTS_VIEW (shortcuts_pane->view),
                                               lp->data))
            {
              show_action = TRUE;
            }
        }

      /* display the action and change the label appropriately */
      g_object_set (G_OBJECT (action),
                    "label", ngettext ("Side Pane (Create Shortcut)", "Side Pane (Create Shortcuts)", n),
                    "sensitive", show_action,
                    "tooltip", ngettext ("Add the selected folder to the shortcuts side pane",
                                         "Add the selected folders to the shortcuts side pane", n),
                    "visible", TRUE,
                    NULL);
    }
  else
    {
      /* hide the action */
      gtk_action_set_visible (action, FALSE);
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (shortcuts_pane), "selected-files");
}



static GtkUIManager*
thunar_shortcuts_pane_get_ui_manager (ThunarComponent *component)
{
  return THUNAR_SHORTCUTS_PANE (component)->ui_manager;
}



static void
thunar_shortcuts_pane_set_ui_manager (ThunarComponent *component,
                                      GtkUIManager    *ui_manager)
{
  ThunarShortcutsPane *shortcuts_pane = THUNAR_SHORTCUTS_PANE (component);
  GError              *error = NULL;

  /* disconnect from the previous UI manager */
  if (G_UNLIKELY (shortcuts_pane->ui_manager != NULL))
    {
      /* drop our action group from the previous UI manager */
      gtk_ui_manager_remove_action_group (shortcuts_pane->ui_manager,
                                          shortcuts_pane->action_group);

      /* unmerge our ui controls from the previous UI manager */
      gtk_ui_manager_remove_ui (shortcuts_pane->ui_manager, shortcuts_pane->ui_merge_id);

      /* drop our reference on the previous UI manager */
      g_object_unref (G_OBJECT (shortcuts_pane->ui_manager));
    }

  /* activate the new UI manager */
  shortcuts_pane->ui_manager = ui_manager;

  /* connect to the new UI manager */
  if (G_LIKELY (ui_manager != NULL))
    {
      /* we keep a reference on the new manager */
      g_object_ref (G_OBJECT (ui_manager));

      /* add our action group to the new manager */
      gtk_ui_manager_insert_action_group (ui_manager, shortcuts_pane->action_group, -1);

      /* merge our UI control items with the new manager */
      shortcuts_pane->ui_merge_id = 
        gtk_ui_manager_add_ui_from_string (ui_manager, thunar_shortcuts_pane_ui,
                                           thunar_shortcuts_pane_ui_length, &error);
      if (G_UNLIKELY (shortcuts_pane->ui_merge_id == 0))
        {
          g_error ("Failed to merge ThunarShortcutsPane menus: %s", error->message);
          g_error_free (error);
        }
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (shortcuts_pane), "ui-manager");
}



static void
thunar_shortcuts_pane_action_shortcuts_add (GtkAction           *action,
                                            ThunarShortcutsPane *shortcuts_pane)
{
  GList *lp;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_PANE (shortcuts_pane));

  /* add all selected folders to the model */
  for (lp = shortcuts_pane->selected_files; lp != NULL; lp = lp->next)
    {
      if (G_LIKELY (thunar_file_is_directory (lp->data)))
        {
          /* add the folder to the shortcuts model */
          thunar_shortcuts_view_add_file (THUNAR_SHORTCUTS_VIEW (shortcuts_pane->view),
                                          lp->data);
        }
    }
}



/**
 * thunar_shortcuts_pane_new:
 *
 * Allocates a new #ThunarShortcutsPane instance.
 *
 * Return value: the newly allocated #ThunarShortcutsPane instance.
 **/
GtkWidget*
thunar_shortcuts_pane_new (void)
{
  return g_object_new (THUNAR_TYPE_SHORTCUTS_PANE, NULL);
}



