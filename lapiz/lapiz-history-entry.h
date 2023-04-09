/*
 * lapiz-history-entry.h
 * This file is part of lapiz
 *
 * Copyright (C) 2006 - Paolo Borelli
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/*
 * Modified by the lapiz Team, 2006. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifndef __LAPIZ_HISTORY_ENTRY_H__
#define __LAPIZ_HISTORY_ENTRY_H__


G_BEGIN_DECLS

#define LAPIZ_TYPE_HISTORY_ENTRY             (lapiz_history_entry_get_type ())
#define LAPIZ_HISTORY_ENTRY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), LAPIZ_TYPE_HISTORY_ENTRY, LapizHistoryEntry))
#define LAPIZ_HISTORY_ENTRY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), LAPIZ_TYPE_HISTORY_ENTRY, LapizHistoryEntryClass))
#define LAPIZ_IS_HISTORY_ENTRY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LAPIZ_TYPE_HISTORY_ENTRY))
#define LAPIZ_IS_HISTORY_ENTRY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_HISTORY_ENTRY))
#define LAPIZ_HISTORY_ENTRY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), LAPIZ_TYPE_HISTORY_ENTRY, LapizHistoryEntryClass))


typedef struct _LapizHistoryEntry        LapizHistoryEntry;
typedef struct _LapizHistoryEntryClass   LapizHistoryEntryClass;
typedef struct _LapizHistoryEntryPrivate LapizHistoryEntryPrivate;

struct _LapizHistoryEntryClass
{
	CtkComboBoxTextClass parent_class;
};

struct _LapizHistoryEntry
{
	CtkComboBoxText parent_instance;

	LapizHistoryEntryPrivate *priv;
};

GType		 lapiz_history_entry_get_type	(void) G_GNUC_CONST;

CtkWidget	*lapiz_history_entry_new		(const gchar       *history_id,
							 gboolean           enable_completion);

void		 lapiz_history_entry_prepend_text	(LapizHistoryEntry *entry,
							 const gchar       *text);

void		 lapiz_history_entry_append_text	(LapizHistoryEntry *entry,
							 const gchar       *text);

void		 lapiz_history_entry_clear		(LapizHistoryEntry *entry);

void		 lapiz_history_entry_set_history_length	(LapizHistoryEntry *entry,
							 guint              max_saved);

guint		 lapiz_history_entry_get_history_length	(LapizHistoryEntry *gentry);

gchar		*lapiz_history_entry_get_history_id	(LapizHistoryEntry *entry);

void             lapiz_history_entry_set_enable_completion
							(LapizHistoryEntry *entry,
							 gboolean           enable);

gboolean         lapiz_history_entry_get_enable_completion
							(LapizHistoryEntry *entry);

CtkWidget	*lapiz_history_entry_get_entry		(LapizHistoryEntry *entry);

typedef gchar * (* LapizHistoryEntryEscapeFunc) (const gchar *str);
void		lapiz_history_entry_set_escape_func	(LapizHistoryEntry *entry,
							 LapizHistoryEntryEscapeFunc escape_func);

G_END_DECLS

#endif /* __LAPIZ_HISTORY_ENTRY_H__ */
