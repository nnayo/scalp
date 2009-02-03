//---------------------
//  Copyright (C) 2000-2006  <Yann GOUY>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; see the file COPYING.  If not, write to
//  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
//  Boston, MA 02111-1307, USA.
//
//  you can write to me at <yann_gouy@yahoo.fr>
//

#include "utils/fifo.h"

#include <avr/interrupt.h>	// cli()
#include <avr/io.h>		// SREG

#include <string.h>		// memcpy()


void FIFO_init(fifo_t *f, void* buf, u16 nb_elem, u16 elem_size)
{
	// set the internals thanks to the provided data
	f->lng = nb_elem;
	f->nb = 0;
	f->elem_size = elem_size;
	f->out = f->in = f->donnees = buf;
}


u8 FIFO_put(fifo_t *f, void* elem)
{
	u8 sreg = SREG;
	cli();

	// if there's at least a free place
	if (f->nb < f->lng) {
		// add the new element
		memcpy(f->in, elem, f->elem_size);
		f->in += f->elem_size;

		// loop back at the end
		if (f->in >= f->donnees + f->lng * f->elem_size)
			f->in = f->donnees;
		f->nb++;

		SREG = sreg;
		return OK;
	} else {
		SREG = sreg;
		return KO;
	}
}


u8 FIFO_get(fifo_t *f, void* elem)
{
	u8 sreg = SREG;
	cli();

	// if there's no element, quit
	if (f->nb == 0) {
		SREG = sreg;
		return KO;
	}

	// get the element
	memcpy(elem, f->out, f->elem_size);
	f->nb--;

	// set the extraction pointer to the next position
	f->out += f->elem_size;
	if (f->out >= f->donnees + f->lng * f->elem_size)
		f->out = f->donnees;

	SREG = sreg;
	return OK;
}


u16 FIFO_free(fifo_t* f)
{
	return (f->lng - f->nb);
}


u16 FIFO_full(fifo_t* f)
{
	return f->nb;
}
