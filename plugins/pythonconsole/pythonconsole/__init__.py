# -*- coding: utf-8 -*-

# __init__.py -- plugin object
#
# Copyright (C) 2006 - Steve Frécinaux
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.

# Parts from "Interactive Python-CTK Console" (stolen from epiphany's console.py)
#     Copyright (C), 1998 James Henstridge <james@daa.com.au>
#     Copyright (C), 2005 Adam Hooper <adamh@densi.com>
# Bits from lapiz Python Console Plugin
#     Copyrignt (C), 2005 Raphaël Slinckx

from gi.repository import GObject, Ctk, Bean, BeanCtk, Lapiz

from .console import PythonConsole
from .config import PythonConsoleConfigWidget
from .config import PythonConsoleConfig

PYTHON_ICON = 'text-x-python'

class PythonConsolePlugin(GObject.Object, Bean.Activatable, BeanCtk.Configurable):
    __gtype_name__ = "PythonConsolePlugin"

    object = GObject.Property(type=GObject.Object)

    def __init__(self):
        GObject.Object.__init__(self)
        self.config_widget = None

    def do_activate(self):
        window = self.object

        self._console = PythonConsole(namespace = {'__builtins__' : __builtins__,
                                             'lapiz' : Lapiz,
                                             'window' : window})
        self._console.eval('print("You can access the main window through ' \
                           '\'window\' :\\n%s" % window)', False)
        bottom = window.get_bottom_panel()
        image = Ctk.Image()
        image.set_from_icon_name(PYTHON_ICON, Ctk.IconSize.MENU)
        bottom.add_item(self._console, _('Python Console'), image)

    def do_deactivate(self):
        window = self.object

        self._console.stop()
        bottom = window.get_bottom_panel()
        bottom.remove_item(self._console)

    def do_create_configure_widget(self):
        if not self.config_widget:
            self.config_widget = PythonConsoleConfigWidget(self.plugin_info.get_data_dir())
        return self.config_widget.configure_widget()

# ex:et:ts=4:
