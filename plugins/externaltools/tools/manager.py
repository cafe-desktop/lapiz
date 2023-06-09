# -*- coding: utf-8 -*-
#    Lapiz External Tools plugin
#    Copyright (C) 2005-2006  Steve Frécinaux <steve@istique.net>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

__all__ = ('Manager', )

import os.path
import re
from .library import *
from .functions import *
import hashlib
from xml.sax import saxutils
from gi.repository import GObject, Gio, Cdk, Ctk, CtkSource, Lapiz

class LanguagesPopup(Ctk.Popover):
    __gtype_name__ = "LanguagesPopup"

    COLUMN_NAME = 0
    COLUMN_ID = 1
    COLUMN_ENABLED = 2

    def __init__(self, widget, languages):
        Ctk.Popover.__init__(self, relative_to=widget)

        self.props.can_focus = True

        self.build()
        self.init_languages(languages)

        self.view.get_selection().select_path((0,))

    def build(self):
        self.model = Ctk.ListStore(str, str, bool)

        self.sw = Ctk.ScrolledWindow()
        self.sw.set_size_request(-1, 200)
        self.sw.show()

        self.sw.set_policy(Ctk.PolicyType.NEVER, Ctk.PolicyType.AUTOMATIC)
        self.sw.set_shadow_type(Ctk.ShadowType.ETCHED_IN)

        self.view = Ctk.TreeView(model=self.model)
        self.view.show()

        self.view.set_headers_visible(False)

        column = Ctk.TreeViewColumn()

        renderer = Ctk.CellRendererToggle()
        column.pack_start(renderer, False)
        column.add_attribute(renderer, 'active', self.COLUMN_ENABLED)

        renderer.connect('toggled', self.on_language_toggled)

        renderer = Ctk.CellRendererText()
        column.pack_start(renderer, True)
        column.add_attribute(renderer, 'text', self.COLUMN_NAME)

        self.view.append_column(column)
        self.view.set_row_separator_func(self.on_separator, None)

        self.sw.add(self.view)

        self.add(self.sw)

    def enabled_languages(self, model, path, piter, ret):
        enabled = model.get_value(piter, self.COLUMN_ENABLED)

        if path.get_indices()[0] == 0 and enabled:
            return True

        if enabled:
            ret.append(model.get_value(piter, self.COLUMN_ID))

        return False

    def languages(self):
        ret = []

        self.model.foreach(self.enabled_languages, ret)
        return ret

    def on_separator(self, model, piter, user_data=None):
        val = model.get_value(piter, self.COLUMN_NAME)
        return val == '-'

    def init_languages(self, languages):
        manager = CtkSource.LanguageManager()
        langs = [manager.get_language(x) for x in manager.get_language_ids()]
        langs.sort(key=lambda x: x.get_name())

        self.model.append([_('All languages'), None, not languages])
        self.model.append(['-', None, False])
        self.model.append([_('Plain Text'), 'plain', 'plain' in languages])
        self.model.append(['-', None, False])

        for lang in langs:
            self.model.append([lang.get_name(), lang.get_id(), lang.get_id() in languages])

    def correct_all(self, model, path, piter, enabled):
        if path.get_indices()[0] == 0:
            return False

        model.set_value(piter, self.COLUMN_ENABLED, enabled)

    def on_language_toggled(self, renderer, path):
        piter = self.model.get_iter(path)

        enabled = self.model.get_value(piter, self.COLUMN_ENABLED)
        self.model.set_value(piter, self.COLUMN_ENABLED, not enabled)

        if path == '0':
            self.model.foreach(self.correct_all, False)
        else:
            self.model.set_value(self.model.get_iter_first(), self.COLUMN_ENABLED, False)


class Manager(GObject.Object):
    TOOL_COLUMN = 0 # For Tree
    NAME_COLUMN = 1 # For Combo

    __gsignals__ = {
        'tools-updated': (GObject.SignalFlags.RUN_LAST, None, ())
    }

    def __init__(self, datadir):
        GObject.Object.__init__(self)
        self.datadir = datadir
        self.dialog = None
        self._size = (0, 0)
        self._languages = {}
        self._tool_rows = {}

        self.build()

    def get_final_size(self):
        return self._size

    def build(self):
        callbacks = {
            'on_new_tool_button_clicked'      : self.on_new_tool_button_clicked,
            'on_remove_tool_button_clicked'   : self.on_remove_tool_button_clicked,
            'on_tool_manager_dialog_response' : self.on_tool_manager_dialog_response,
            'on_tool_manager_dialog_configure_event': self.on_tool_manager_dialog_configure_event,
            'on_tool_manager_dialog_focus_out': self.on_tool_manager_dialog_focus_out,
            'on_accelerator_key_press'        : self.on_accelerator_key_press,
            'on_accelerator_focus_in'         : self.on_accelerator_focus_in,
            'on_accelerator_focus_out'        : self.on_accelerator_focus_out,
            'on_languages_button_clicked'     : self.on_languages_button_clicked
        }

        # Load the "main-window" widget from the ui file.
        self.ui = Ctk.Builder()
        self.ui.add_from_file(os.path.join(self.datadir, 'ui', 'tools.ui'))
        self.ui.connect_signals(callbacks)
        self.dialog = self.ui.get_object('tool-manager-dialog')

        self.view = self.ui.get_object('view')

        self.__init_tools_model()
        self.__init_tools_view()

        for name in ['input', 'output', 'applicability', 'save-files']:
            self.__init_combobox(name)

        self.do_update()

    def expand_from_doc(self, doc):
        row = None

        if doc:
            if doc.get_language():
                lid = doc.get_language().get_id()

                if lid in self._languages:
                    row = self._languages[lid]
            elif 'plain' in self._languages:
                row = self._languages['plain']

        if not row and None in self._languages:
            row = self._languages[None]

        if not row:
            return

        self.view.expand_row(row.get_path(), False)
        self.view.get_selection().select_path(row.get_path())

    def run(self, window):
        if self.dialog == None:
            self.build()

        # Open up language
        self.expand_from_doc(window.get_active_document())

        self.dialog.set_transient_for(window)
        window.get_group().add_window(self.dialog)
        self.dialog.present()

    def add_accelerator(self, item):
        if not item.shortcut:
            return

        if item.shortcut in self.accelerators:
            if not item in self.accelerators[item.shortcut]:
                self.accelerators[item.shortcut].append(item)
        else:
            self.accelerators[item.shortcut] = [item]

    def remove_accelerator(self, item, shortcut=None):
        if not shortcut:
            shortcut = item.shortcut

        if not shortcut in self.accelerators:
            return

        self.accelerators[shortcut].remove(item)

        if not self.accelerators[shortcut]:
            del self.accelerators[shortcut]

    def add_tool_to_language(self, tool, language):
        if isinstance(language, CtkSource.Language):
            lid = language.get_id()
        else:
            lid = language

        if not lid in self._languages:
            piter = self.model.append(None, [language])

            parent = Ctk.TreeRowReference.new(self.model, self.model.get_path(piter))
            self._languages[lid] = parent
        else:
            parent = self._languages[lid]

        piter = self.model.get_iter(parent.get_path())
        child = self.model.append(piter, [tool])

        if not tool in self._tool_rows:
            self._tool_rows[tool] = []

        self._tool_rows[tool].append(Ctk.TreeRowReference.new(self.model, self.model.get_path(child)))
        return child

    def add_tool(self, tool):
        manager = CtkSource.LanguageManager()
        ret = None

        for lang in tool.languages:
            l = manager.get_language(lang)

            if l:
                ret = self.add_tool_to_language(tool, l)
            elif lang == 'plain':
                ret = self.add_tool_to_language(tool, 'plain')

        if not ret:
            ret = self.add_tool_to_language(tool, None)

        self.add_accelerator(tool)
        return ret

    def __init_tools_model(self):
        self.tools = ToolLibrary()
        self.current_node = None
        self.script_hash = None
        self.accelerators = dict()

        self.model = Ctk.TreeStore(object)
        self.view.set_model(self.model)

        for tool in self.tools.tree.tools:
            self.add_tool(tool)

        self.model.set_default_sort_func(self.sort_tools)
        self.model.set_sort_column_id(-1, Ctk.SortType.ASCENDING)

    def sort_tools(self, model, iter1, iter2, user_data=None):
        # For languages, sort All before everything else, otherwise alphabetical
        t1 = model.get_value(iter1, self.TOOL_COLUMN)
        t2 = model.get_value(iter2, self.TOOL_COLUMN)

        if model.iter_parent(iter1) == None:
            if t1 == None:
                return -1

            if t2 == None:
                return 1

            def lang_name(lang):
                if isinstance(lang, CtkSource.Language):
                    return lang.get_name()
                else:
                    return _('Plain Text')

            n1 = lang_name(t1)
            n2 = lang_name(t2)
        else:
            n1 = t1.name
            n2 = t2.name

        n1 = n1.lower()
        n2 = n2.lower()
        return (n1 > n2) - (n1 < n2)

    def __init_tools_view(self):
        # Tools column
        column = Ctk.TreeViewColumn('Tools')
        renderer = Ctk.CellRendererText()
        column.pack_start(renderer, False)
        renderer.set_property('editable', True)
        self.view.append_column(column)

        column.set_cell_data_func(renderer, self.get_cell_data_cb, None)

        renderer.connect('edited', self.on_view_label_cell_edited)
        renderer.connect('editing-started', self.on_view_label_cell_editing_started)

        self.selection_changed_id = self.view.get_selection().connect('changed', self.on_view_selection_changed, None)

    def __init_combobox(self, name):
        combo = self[name]
        combo.set_active(0)

    # Convenience function to get an object from its name
    def __getitem__(self, key):
        return self.ui.get_object(key)

    def set_active_by_name(self, combo_name, option_name):
        combo = self[combo_name]
        model = combo.get_model()
        piter = model.get_iter_first()
        while piter is not None:
            if model.get_value(piter, self.NAME_COLUMN) == option_name:
                combo.set_active_iter(piter)
                return True
            piter = model.iter_next(piter)
        return False

    def get_selected_tool(self):
        model, piter = self.view.get_selection().get_selected()

        if piter is not None:
            tool = model.get_value(piter, self.TOOL_COLUMN)

            if not isinstance(tool, Tool):
                tool = None

            return piter, tool
        else:
            return None, None

    def compute_hash(self, stringofbytes):
        return hashlib.md5(stringofbytes).hexdigest()

    def save_current_tool(self):
        if self.current_node is None:
             return

        if self.current_node.filename is None:
            self.current_node.autoset_filename()

        def combo_value(o, name):
            combo = o[name]
            return combo.get_model().get_value(combo.get_active_iter(), self.NAME_COLUMN)

        self.current_node.input = combo_value(self, 'input')
        self.current_node.output = combo_value(self, 'output')
        self.current_node.applicability = combo_value(self, 'applicability')
        self.current_node.save_files = combo_value(self, 'save-files')

        buf = self['commands'].get_buffer()
        (start, end) = buf.get_bounds()
        script  = buf.get_text(start, end, False)
        scriptbytes = script
        if not isinstance(scriptbytes, bytes):
            scriptbytes = scriptbytes.encode('utf-8')
        h = self.compute_hash(scriptbytes)
        if h != self.script_hash:
            # script has changed -> save it
            self.current_node.save_with_script([line + "\n" for line in script.splitlines()])
            self.script_hash = h
        else:
            self.current_node.save()

        self.update_remove_revert()

    def clear_fields(self):
        self['accelerator'].set_text('')

        buf = self['commands'].get_buffer()
        buf.begin_not_undoable_action()
        buf.set_text('')
        buf.end_not_undoable_action()

        for nm in ('input', 'output', 'applicability', 'save-files'):
            self[nm].set_active(0)

        self['languages_label'].set_text(_('All Languages'))

    def fill_languages_button(self):
        if not self.current_node or not self.current_node.languages:
            self['languages_label'].set_text(_('All Languages'))
        else:
            manager = CtkSource.LanguageManager()
            langs = []

            for lang in self.current_node.languages:
                if lang == 'plain':
                    langs.append(_('Plain Text'))
                else:
                    l = manager.get_language(lang)

                    if l:
                        langs.append(l.get_name())

            self['languages_label'].set_text(', '.join(langs))

    def fill_fields(self):
        node = self.current_node
        self['accelerator'].set_text(default(node.shortcut, ''))

        buf = self['commands'].get_buffer()
        script = default(''.join(node.get_script()), '')

        buf.begin_not_undoable_action()
        buf.set_text(script)
        buf.end_not_undoable_action()

        if not isinstance(script, bytes):
            script = script.encode('utf-8')

        self.script_hash = self.compute_hash(script)
        contenttype, uncertain = Gio.content_type_guess(None, script)
        lmanager = CtkSource.LanguageManager.get_default()
        language = lmanager.guess_language(content_type=contenttype)

        if language is not None:
            buf.set_language(language)
            buf.set_highlight_syntax(True)
        else:
            buf.set_highlight_syntax(False)

        for nm in ('input', 'output', 'applicability', 'save-files'):
            model = self[nm].get_model()
            piter = model.get_iter_first()

            self.set_active_by_name(nm,
                                    default(node.__getattribute__(nm.replace('-', '_')),
                                    model.get_value(piter, self.NAME_COLUMN)))

        self.fill_languages_button()

    def update_remove_revert(self):
        piter, node = self.get_selected_tool()

        removable = node is not None and node.is_local()

        self['remove-tool-button'].set_sensitive(removable)
        self['revert-tool-button'].set_sensitive(removable)

        if node is not None and node.is_global():
            self['remove-tool-button'].hide()
            self['revert-tool-button'].show()
        else:
            self['remove-tool-button'].show()
            self['revert-tool-button'].hide()

    def do_update(self):
        self.update_remove_revert()

        piter, node = self.get_selected_tool()
        self.current_node = node

        if node is not None:
            self.fill_fields()
            self['tool-table'].set_sensitive(True)
        else:
            self.clear_fields()
            self['tool-table'].set_sensitive(False)

    def language_id_from_iter(self, piter):
        if not piter:
            return None

        tool = self.model.get_value(piter, self.TOOL_COLUMN)

        if isinstance(tool, Tool):
            piter = self.model.iter_parent(piter)
            tool = self.model.get_value(piter, self.TOOL_COLUMN)

        if isinstance(tool, CtkSource.Language):
            return tool.get_id()
        elif tool:
            return 'plain'

        return None

    def selected_language_id(self):
        # Find current language if there is any
        model, piter = self.view.get_selection().get_selected()

        return self.language_id_from_iter(piter)

    def on_new_tool_button_clicked(self, button):
        self.save_current_tool()

        # block handlers while inserting a new item
        self.view.get_selection().handler_block(self.selection_changed_id)

        self.current_node = Tool(self.tools.tree);
        self.current_node.name = _('New tool')
        self.tools.tree.tools.append(self.current_node)

        lang = self.selected_language_id()

        if lang:
            self.current_node.languages = [lang]

        piter = self.add_tool(self.current_node)

        self.view.set_cursor(self.model.get_path(piter), self.view.get_column(self.TOOL_COLUMN), True)
        self.fill_fields()

        self['tool-table'].set_sensitive(True)
        self.view.get_selection().handler_unblock(self.selection_changed_id)

    def tool_changed(self, tool, refresh=False):
        for row in self._tool_rows[tool]:
            self.model.row_changed(row.get_path(), self.model.get_iter(row.get_path()))

        if refresh and tool == self.current_node:
            self.fill_fields()

        self.update_remove_revert()

    def on_remove_tool_button_clicked(self, button):
        piter, node = self.get_selected_tool()

        if not node:
            return

        if node.is_global():
            shortcut = node.shortcut

            if node.parent.revert_tool(node):
                self.remove_accelerator(node, shortcut)
                self.add_accelerator(node)

                self['revert-tool-button'].set_sensitive(False)
                self.fill_fields()

                self.tool_changed(node)
        else:
            parent = self.model.iter_parent(piter)
            language = self.language_id_from_iter(parent)

            self.model.remove(piter)

            if language in node.languages:
                node.languages.remove(language)

            self._tool_rows[node] = [x for x in self._tool_rows[node] if x.valid()]

            if not self._tool_rows[node]:
                del self._tool_rows[node]

                if node.parent.delete_tool(node):
                    self.remove_accelerator(node)
                    self.current_node = None
                    self.script_hash = None

                    if self.model.iter_is_valid(piter):
                        self.view.set_cursor(self.model.get_path(piter),
                                             self.view.get_column(self.TOOL_COLUMN),
                                             False)

                self.view.grab_focus()

            path = self._languages[language].get_path()
            parent = self.model.get_iter(path)

            if not self.model.iter_has_child(parent):
                self.model.remove(parent)
                del self._languages[language]

    def on_view_label_cell_edited(self, cell, path, new_text):
        if new_text != '':
            piter = self.model.get_iter(path)
            tool = self.model.get_value(piter, self.TOOL_COLUMN)

            tool.name = new_text

            self.save_current_tool()
            self.tool_changed(tool)

    def on_view_label_cell_editing_started(self, renderer, editable, path):
            piter = self.model.get_iter(path)
            tool = self.model.get_value(piter, self.TOOL_COLUMN)

            if isinstance(editable, Ctk.Entry):
                editable.set_text(tool.name)
                editable.grab_focus()

    def on_view_selection_changed(self, selection, userdata):
        self.save_current_tool()
        self.do_update()

    def accelerator_collision(self, name, node):
        if not name in self.accelerators:
            return []

        ret = []

        for other in self.accelerators[name]:
            if not other.languages or not node.languages:
                ret.append(other)
                continue

            for lang in other.languages:
                if lang in node.languages:
                    ret.append(other)
                    continue

        return ret

    def set_accelerator(self, keyval, mod):
        # Check whether accelerator already exists
        self.remove_accelerator(self.current_node)

        name = Ctk.accelerator_name(keyval, mod)

        if name == '':
            self.current_node.shorcut = None
            self.save_current_tool()
            return True

        col = self.accelerator_collision(name, self.current_node)

        if col:
            dialog = Ctk.MessageDialog(self.dialog,
                                       Ctk.DialogFlags.MODAL,
                                       Ctk.MessageType.ERROR,
                                       Ctk.ButtonsType.CLOSE,
                                       _('This accelerator is already bound to %s') % (', '.join(map(lambda x: x.name, col)),))

            dialog.run()
            dialog.destroy()

            self.add_accelerator(self.current_node)
            return False

        self.current_node.shortcut = name
        self.add_accelerator(self.current_node)
        self.save_current_tool()

        return True

    def on_accelerator_key_press(self, entry, event):
        mask = event.state & Ctk.accelerator_get_default_mod_mask()
        keyname = Cdk.keyval_name(event.keyval)

        if keyname == 'Escape':
            entry.set_text(default(self.current_node.shortcut, ''))
            self['commands'].grab_focus()
            return True
        elif keyname == 'Delete' or keyname == 'BackSpace':
            entry.set_text('')
            self.remove_accelerator(self.current_node)
            self.current_node.shortcut = None
            self['commands'].grab_focus()
            return True
        elif re.match('^F(:1[012]?|[2-9])$', keyname):
            # New accelerator
            if self.set_accelerator(event.keyval, mask):
                entry.set_text(default(self.current_node.shortcut, ''))
                self['commands'].grab_focus()

            # Capture all `normal characters`
            return True
        elif Cdk.keyval_to_unicode(event.keyval):
            if mask:
                # New accelerator
                if self.set_accelerator(event.keyval, mask):
                    entry.set_text(default(self.current_node.shortcut, ''))
                    self['commands'].grab_focus()
            # Capture all `normal characters`
            return True
        else:
            return False

    def on_accelerator_focus_in(self, entry, event):
        if self.current_node is None:
            return
        if self.current_node.shortcut:
            entry.set_text(_('Type a new accelerator, or press Backspace to clear'))
        else:
            entry.set_text(_('Type a new accelerator'))

    def on_accelerator_focus_out(self, entry, event):
        if self.current_node is not None:
            entry.set_text(default(self.current_node.shortcut, ''))
            self.tool_changed(self.current_node)

    def on_tool_manager_dialog_response(self, dialog, response):
        if response == Ctk.ResponseType.HELP:
            Lapiz.help_display(self.dialog, 'lapiz', 'lapiz-external-tools-plugin')
            return

        self.on_tool_manager_dialog_focus_out(dialog, None)

        self.dialog.destroy()
        self.dialog = None
        self.tools = None

    def on_tool_manager_dialog_configure_event(self, dialog, event):
        if dialog.get_realized():
            alloc = dialog.get_allocation()
            self._size = (alloc.width, alloc.height)

    def on_tool_manager_dialog_focus_out(self, dialog, event):
        self.save_current_tool()
        self.emit('tools-updated')

    def get_cell_data_cb(self, column, cell, model, piter, user_data=None):
        tool = model.get_value(piter, self.TOOL_COLUMN)

        if tool == None or not isinstance(tool, Tool):
            if tool == None:
                label = _('All Languages')
            elif not isinstance(tool, CtkSource.Language):
                label = _('Plain Text')
            else:
                label = tool.get_name()

            markup = saxutils.escape(label)
            editable = False
        else:
            escaped = saxutils.escape(tool.name)

            if tool.shortcut:
                markup = '%s (<b>%s</b>)' % (escaped, saxutils.escape(tool.shortcut))
            else:
                markup = escaped

            editable = True

        cell.set_properties(markup=markup, editable=editable)

    def tool_in_language(self, tool, lang):
        if not lang in self._languages:
            return False

        ref = self._languages[lang]
        parent = ref.get_path()

        for row in self._tool_rows[tool]:
            path = row.get_path()

            if path.get_indices()[0] == parent.get_indices()[0]:
                return True

        return False

    def update_languages(self, popup):
        self.current_node.languages = popup.languages()
        self.fill_languages_button()

        piter, node = self.get_selected_tool()
        ret = None

        if node:
            ref = Ctk.TreeRowReference.new(self.model, self.model.get_path(piter))

        # Update languages, make sure to inhibit selection change stuff
        self.view.get_selection().handler_block(self.selection_changed_id)

        # Remove all rows that are no longer
        for row in list(self._tool_rows[self.current_node]):
            piter = self.model.get_iter(row.get_path())
            language = self.language_id_from_iter(piter)

            if (not language and not self.current_node.languages) or \
               (language in self.current_node.languages):
                continue

            # Remove from language
            self.model.remove(piter)
            self._tool_rows[self.current_node].remove(row)

            # If language is empty, remove it
            parent = self.model.get_iter(self._languages[language].get_path())

            if not self.model.iter_has_child(parent):
                self.model.remove(parent)
                del self._languages[language]

        # Now, add for any that are new
        manager = CtkSource.LanguageManager()

        for lang in self.current_node.languages:
            if not self.tool_in_language(self.current_node, lang):
                l = manager.get_language(lang)

                if not l:
                    l = 'plain'

                self.add_tool_to_language(self.current_node, l)

        if not self.current_node.languages and not self.tool_in_language(self.current_node, None):
            self.add_tool_to_language(self.current_node, None)

        # Check if we can still keep the current
        if not ref or not ref.valid():
            # Change selection to first language
            path = self._tool_rows[self.current_node][0].get_path()
            piter = self.model.get_iter(path)
            parent = self.model.iter_parent(piter)

            # Expand parent, select child and scroll to it
            self.view.expand_row(self.model.get_path(parent), False)
            self.view.get_selection().select_path(path)
            self.view.set_cursor(path, self.view.get_column(self.TOOL_COLUMN), False)

        self.view.get_selection().handler_unblock(self.selection_changed_id)

    def on_languages_button_clicked(self, button):
        popup = LanguagesPopup(button, self.current_node.languages)
        popup.show()
        popup.connect('closed', self.update_languages)

# ex:et:ts=4:
