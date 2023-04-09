/*
 * lapiz-progress-message-area.h
 * This file is part of lapiz
 *
 * Copyright (C) 2005 - Paolo Maggi
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
 * Modified by the lapiz Team, 2005. See the AUTHORS file for a
 * list of people on the lapiz Team.
 * See the ChangeLog files for a list of changes.
 *
 * $Id$
 */

#ifndef __LAPIZ_PROGRESS_MESSAGE_AREA_H__
#define __LAPIZ_PROGRESS_MESSAGE_AREA_H__

#include <ctk/ctk.h>

G_BEGIN_DECLS

/*
 * Type checking and casting macros
 */
#define LAPIZ_TYPE_PROGRESS_MESSAGE_AREA              (lapiz_progress_message_area_get_type())
#define LAPIZ_PROGRESS_MESSAGE_AREA(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), LAPIZ_TYPE_PROGRESS_MESSAGE_AREA, LapizProgressMessageArea))
#define LAPIZ_PROGRESS_MESSAGE_AREA_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), LAPIZ_TYPE_PROGRESS_MESSAGE_AREA, LapizProgressMessageAreaClass))
#define LAPIZ_IS_PROGRESS_MESSAGE_AREA(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), LAPIZ_TYPE_PROGRESS_MESSAGE_AREA))
#define LAPIZ_IS_PROGRESS_MESSAGE_AREA_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), LAPIZ_TYPE_PROGRESS_MESSAGE_AREA))
#define LAPIZ_PROGRESS_MESSAGE_AREA_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), LAPIZ_TYPE_PROGRESS_MESSAGE_AREA, LapizProgressMessageAreaClass))

/* Private structure type */
typedef struct _LapizProgressMessageAreaPrivate LapizProgressMessageAreaPrivate;

/*
 * Main object structure
 */
typedef struct _LapizProgressMessageArea LapizProgressMessageArea;

struct _LapizProgressMessageArea
{
	CtkInfoBar parent;

	/*< private > */
	LapizProgressMessageAreaPrivate *priv;
};

/*
 * Class definition
 */
typedef struct _LapizProgressMessageAreaClass LapizProgressMessageAreaClass;

struct _LapizProgressMessageAreaClass
{
	CtkInfoBarClass parent_class;
};

/*
 * Public methods
 */
GType 		 lapiz_progress_message_area_get_type 		(void) G_GNUC_CONST;

CtkWidget	*lapiz_progress_message_area_new      		(const gchar              *image_id,
								 const gchar              *markup,
								 gboolean                  has_cancel);

void		 lapiz_progress_message_area_set_image		(LapizProgressMessageArea *area,
								 const gchar              *image_id);

void		 lapiz_progress_message_area_set_markup		(LapizProgressMessageArea *area,
								 const gchar              *markup);

void		 lapiz_progress_message_area_set_text		(LapizProgressMessageArea *area,
								 const gchar              *text);

void		 lapiz_progress_message_area_set_fraction	(LapizProgressMessageArea *area,
								 gdouble                   fraction);

void		 lapiz_progress_message_area_pulse		(LapizProgressMessageArea *area);


G_END_DECLS

#endif  /* __LAPIZ_PROGRESS_MESSAGE_AREA_H__  */
