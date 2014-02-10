/* Copyright (C) 2003-2013 Runtime Revolution Ltd.
 
 This file is part of LiveCode.
 
 LiveCode is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License v3 as published by the Free
 Software Foundation.
 
 LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 for more details.
 
 You should have received a copy of the GNU General Public License
 along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

#include "platform.h"

#include "core.h"
#include "globdefs.h"
#include "filedefs.h"
#include "osspec.h"
#include "typedefs.h"
#include "parsedef.h"
#include "objdefs.h"

#include "execpt.h"
#include "scriptpt.h"
#include "mcerror.h"
#include "globals.h"
#include "util.h"
#include "stack.h"
#include "card.h"
#include "debug.h"
#include "scrolbar.h"
#include "button.h"
#include "resolution.h"
#include "redraw.h"
#include "notify.h"
#include "dispatch.h"
#include "image.h"
#include "field.h"
#include "styledtext.h"
#include "graphicscontext.h"
#include "region.h"

#include "desktop-dc.h"

#include "platform.h"

#include <Carbon/Carbon.h>

////////////////////////////////////////////////////////////////////////////////

Boolean tripleclick = False;

MCDisplay *MCScreenDC::s_monitor_displays = nil;
uint4 MCScreenDC::s_monitor_count = 0;

////////////////////////////////////////////////////////////////////////////////

MCScreenDC::MCScreenDC(void)
{
	MCNotifyInitialize();
}

MCScreenDC::~MCScreenDC(void)
{
}

bool MCScreenDC::hasfeature(MCPlatformFeature p_feature)
{
	switch(p_feature)
	{
		case PLATFORM_FEATURE_WINDOW_TRANSPARENCY:
		case PLATFORM_FEATURE_OS_COLOR_DIALOGS:
		case PLATFORM_FEATURE_OS_FILE_DIALOGS:
		case PLATFORM_FEATURE_OS_PRINT_DIALOGS:
			return true;
			break;
			
		case PLATFORM_FEATURE_TRANSIENT_SELECTION:
			return false;
			break;
			
		default:
			assert(false);
			break;
	}
	
	return false;
}

Boolean MCScreenDC::open()
{
	black_pixel.red = black_pixel.green = black_pixel.blue = 0; //black pixel
	white_pixel.red = white_pixel.green = white_pixel.blue = 0xFFFF; //white pixel
	black_pixel.pixel = 0;
	white_pixel.pixel = 0xFFFFFF;
	
	MCzerocolor = MCbrushcolor = white_pixel;
	alloccolor(MCbrushcolor);
	MCselectioncolor = MCpencolor = black_pixel;
	alloccolor(MCselectioncolor);
	alloccolor(MCpencolor);
	gray_pixel.red = gray_pixel.green = gray_pixel.blue = 0x8888;
	alloccolor(gray_pixel);
	background_pixel.red = background_pixel.green = background_pixel.blue = 0xffff;
	alloccolor(background_pixel);

	MCPlatformGetSystemProperty(kMCPlatformSystemPropertyHiliteColor, kMCPlatformPropertyTypeColor, &MChilitecolor);
	alloccolor(MChilitecolor);
	
	MCPlatformGetSystemProperty(kMCPlatformSystemPropertyAccentColor, kMCPlatformPropertyTypeColor, &MCaccentcolor);
	alloccolor(MCaccentcolor);
	
	MCPlatformGetSystemProperty(kMCPlatformSystemPropertyDoubleClickInterval, kMCPlatformPropertyTypeUInt16, &MCdoubletime);
	
	MCtemplatescrollbar->alloccolors();
	MCtemplatebutton->allocicons();
	
	MCcursors[PI_NONE] = nil;
	
	MCPlatformGetSystemProperty(kMCPlatformSystemPropertyCaretBlinkInterval, kMCPlatformPropertyTypeDouble, &MCblinkrate);
			
	MCDisplay const *t_displays;
	getdisplays(t_displays, false);
	MCwbr = t_displays[0] . workarea;
	
	backdrop_enabled = false;
	backdrop_pattern = nil;
	MCPlatformCreateWindow(backdrop_window);
	MCPlatformConfigureBackdrop(backdrop_window);

	return True;
}

Boolean MCScreenDC::close(Boolean force)
{
	// COCOA-TODO: Is this still needed?
	uint2 i;
	if (ncolors != 0)
	{
		int2 i;
		for (i = 0 ; i < ncolors ; i++)
		{
			if (colornames[i] != NULL)
				delete colornames[i];
		}
		delete colors;
		delete colornames;
		delete allocs;
	}
	
	return True;
}

const char *MCScreenDC::getdisplayname()
{
	return "local Mac";
}

uint2 MCScreenDC::getmaxpoints(void)
{
	return 32767;
}

uint2 MCScreenDC::getvclass(void)
{
	return DirectColor;
}

uint2 MCScreenDC::getdepth(void)
{
	return 32;
}

void MCScreenDC::getvendorstring(MCExecPoint &ep)
{
	ep.setstaticcstring("Mac OS");
}

uint2 MCScreenDC::getpad()
{
	return 32;
}

// This method is no longer relevant - it only ever worked on Mac and
// even then only on mac classic.
MCColor *MCScreenDC::getaccentcolors()
{
	return nil;
}

// IM-2013-08-01: [[ ResIndependence ]] refactored methods that return device coordinates
uint16_t MCScreenDC::device_getwidth(void)
{
	MCRectangle t_viewport;
	MCPlatformGetScreenViewport(0, t_viewport);
	return t_viewport . width;
}

uint16_t MCScreenDC::device_getheight(void)
{
	MCRectangle t_viewport;
	MCPlatformGetScreenViewport(0, t_viewport);
	return t_viewport . height;
}

bool MCScreenDC::device_getdisplays(bool p_effective, MCDisplay *& r_displays, uint32_t &r_count)
{
	bool t_success;
	t_success = true;
	
	MCDisplay *t_displays;
	t_displays = nil;
	
	uint32_t t_display_count;
	t_display_count = 0;
	
	if (s_monitor_count != 0)
	{
		t_displays = s_monitor_displays;
		t_display_count = s_monitor_count;
	}
	else
	{
		MCPlatformGetScreenCount(t_display_count);
		
		if (t_success)
			t_success = MCMemoryNewArray(t_display_count, t_displays);
		
		if (t_success)
		{
			for(uindex_t i = 0; i < t_display_count; i++)
			{
				t_displays[i] . index = i;
				MCPlatformGetScreenViewport(i, t_displays[i] . device_viewport);
				MCPlatformGetScreenWorkarea(i, t_displays[i] . device_workarea);
			}
		}
		
		if (t_success)
		{
			s_monitor_count = t_display_count;
			s_monitor_displays = t_displays;
		}
		else
			MCMemoryDeleteArray(t_displays);
	}

	r_displays = t_displays;
	r_count = t_display_count;
	
	return true;
}

void MCScreenDC::device_boundrect(MCRectangle &rect, Boolean title, Window_mode mode)
{
	MCRectangle srect;
	
	if (mode >= WM_MODAL)
	{
		const MCDisplay *t_display;
		t_display = getnearestdisplay(rect);
		srect = t_display -> device_workarea;
	}
	else
		srect = MCGRectangleGetIntegerInterior(MCResUserToDeviceRect(MCwbr));
	
	uint2 sr, sw, sb, sh;
	
	// COCOA-TODO: This is Mac specific
	Rect screenRect;
	SetRect(&screenRect, srect . x, srect . y, srect . x + srect . width, srect . y + srect . height);
	
	if (title && mode <= WM_SHEET && mode != WM_DRAWER)
	{
		// COCOA-TODO: These values should be queryable (once we figure out what
		//   'title' is meant to do...)
		if (mode == WM_PALETTE)
			screenRect.top += 13;
		else
		{
			screenRect.top += 22;
		}
		sr = sb = 10;
		sw = 20;
		sh = 12;
	}
	else
		sr = sw = sb = sh = 0;
	
	if (rect.x < screenRect.left)
		rect.x = screenRect.left;
	if (rect.x + rect.width > screenRect.right - sr)
	{
		if (rect.width > screenRect.right - screenRect.left - sw)
			rect.width = screenRect.right - screenRect.left - sw;
		rect.x = screenRect.right - rect.width - sr;
	}
	
	if (rect.y < screenRect.top)
		rect.y = screenRect.top;
	if (rect.y + rect.height > screenRect.bottom - sb)
	{
		if (rect.height > screenRect.bottom - screenRect.top - sh)
			rect.height = screenRect.bottom - screenRect.top - sh;
		rect.y = screenRect.bottom - rect.height - sb;
	}
}

////////////////////////////////////////////////////////////////////////////////

static MCPlatformStandardCursor theme_cursorlist[PI_NCURSORS] =
{
	kMCPlatformStandardCursorArrow, kMCPlatformStandardCursorArrow,
	kMCPlatformStandardCursorArrow, kMCPlatformStandardCursorArrow, kMCPlatformStandardCursorArrow, kMCPlatformStandardCursorWatch, kMCPlatformStandardCursorWatch,
	kMCPlatformStandardCursorCross, kMCPlatformStandardCursorArrow, kMCPlatformStandardCursorIBeam, kMCPlatformStandardCursorArrow, kMCPlatformStandardCursorArrow,
	kMCPlatformStandardCursorArrow, kMCPlatformStandardCursorCross, kMCPlatformStandardCursorWatch, kMCPlatformStandardCursorArrow   
};

void MCScreenDC::resetcursors()
{
	// MW-2010-09-10: Make sure no stacks reference one of the standard cursors.
	MCdispatcher -> clearcursors();

	int32_t t_cursor_size;
	MCPlatformGetSystemProperty(kMCPlatformSystemPropertyMaximumCursorSize, kMCPlatformPropertyTypeInt32, &t_cursor_size);
	MCcursormaxsize = t_cursor_size;
	
	MCPlatformCursorImageSupport t_image_support;
	MCPlatformGetSystemProperty(kMCPlatformSystemPropertyCursorImageSupport, kMCPlatformPropertyTypeCursorImageSupport, &t_image_support);
	MCcursorcanbealpha = MCcursorcanbecolor = MCcursorbwonly = False;
	switch(t_image_support)
	{
	case kMCPlatformCursorImageSupportAlpha:
		MCcursorcanbealpha = True;
	case kMCPlatformCursorImageSupportColor:
		MCcursorcanbecolor = True;
		break;
	case kMCPlatformCursorImageSupportBilevel:
		MCcursorbwonly = False;
		break;
	case kMCPlatformCursorImageSupportMonochrome:
		MCcursorbwonly = True;
		break;
	};
	
	for(uindex_t i = 0; i < PI_NCURSORS; i++)
	{
		freecursor(MCcursors[i]);
		MCcursors[i] = nil;
		
		MCImage *im;
		if (i == PI_NONE)
			MCcursors[i] = nil;
		else if ((im = (MCImage *)MCdispatcher->getobjid(CT_IMAGE, i)) != NULL)
			MCcursors[i] = im -> createcursor();
		else if (i < PI_BUSY1)
			MCPlatformCreateStandardCursor(theme_cursorlist[i], MCcursors[i]);
		else
			MCPlatformCreateStandardCursor(kMCPlatformStandardCursorWatch, MCcursors[i]);
	}
}

void MCScreenDC::setcursor(Window w, MCCursorRef c)
{
	// Disable cursor setting when we are a drag-target
	if (MCdispatcher -> isdragtarget())
		return;
		
	if (c == nil)
		MCPlatformHideCursor();
	else
		MCPlatformShowCursor(c);
}

MCCursorRef MCScreenDC::createcursor(MCImageBitmap *image, int2 xhot, int2 yhot)
{
	MCCursorRef t_cursor;
	MCPlatformCreateCustomCursor(image, MCPointMake(xhot, yhot), t_cursor);
	return t_cursor;
}

void MCScreenDC::freecursor(MCCursorRef c)
{
	if (c == nil)
		return;
	
	MCPlatformReleaseCursor(c);
}

////////////////////////////////////////////////////////////////////////////////

bool MCScreenDC::device_getwindowgeometry(Window w, MCRectangle &drect)
{
	if (w == nil)
		return false;
	MCPlatformGetWindowContentRect(w, drect);
	return true;
}

void MCScreenDC::openwindow(Window w, Boolean override)
{
	MCStack *t_stack;
	t_stack = MCdispatcher -> findstackd(w);
	if (t_stack == nil)
		return;
	
	MCPlatformWindowRef t_parent;
	if (t_stack != nil)
		t_parent = t_stack -> getparentwindow();
		
	if (t_stack -> getmode() != WM_SHEET)
		MCPlatformShowWindow(w);
	else
		MCPlatformShowWindowAsSheet(w, t_parent);
}

void MCScreenDC::closewindow(Window window)
{
	MCPlatformHideWindow(window);
}

void MCScreenDC::destroywindow(Window &window)
{
	MCPlatformReleaseWindow(window);
	window = nil;
}

void MCScreenDC::raisewindow(Window window)
{
	MCPlatformRaiseWindow(window);
}

void MCScreenDC::iconifywindow(Window window)
{
	MCPlatformIconifyWindow(window);
}

void MCScreenDC::uniconifywindow(Window window)
{
	MCPlatformUniconifyWindow(window);
}

void MCScreenDC::setname(Window window, const char *newname)
{
	MCPlatformSetWindowProperty(window, kMCPlatformWindowPropertyTitle, kMCPlatformPropertyTypeUTF8CString, &newname);
}

void MCScreenDC::setinputfocus(Window window)
{
	MCPlatformFocusWindow(window);
}

uint4 MCScreenDC::dtouint4(Drawable d)
{
	if (d == nil)
		return 0;
	
	MCPlatformWindowRef t_window;
	t_window = (MCPlatformWindowRef)d;
	
	uint32_t t_id;
	MCPlatformGetWindowProperty(t_window, kMCPlatformWindowPropertySystemId, kMCPlatformPropertyTypeUInt32, &t_id);
	
	return t_id;
}

Boolean MCScreenDC::uint4towindow(uint4, Window &w)
{
	return False;
}

////////////////////////////////////////////////////////////////////////////////

void MCScreenDC::seticon(uint4 p_icon)
{
}

void MCScreenDC::seticonmenu(const char *p_menu)
{
}

void MCScreenDC::configurestatusicon(uint32_t icon_id, const char *menu, const char *tooltip)
{
}

////////////////////////////////////////////////////////////////////////////////

void MCScreenDC::enactraisewindows(void)
{
	// If the backdrop is already enabled, there is nothing to do.
	if (backdrop_enabled)
		return;
	
	// If raiseWindows mode is active and there is no backdrop then resize the
	// backdrop window and set it.
	if (MCraisewindows)
	{
		MCRectangle t_rect;
		t_rect = MCRectangleMake(0, 0, 0, 0);
		MCPlatformSetWindowProperty(backdrop_window, kMCPlatformWindowPropertyFrameRect, kMCPlatformPropertyTypeRectangle, &t_rect);
		MCPlatformShowWindow(backdrop_window);
	}
	else
	{
		MCPlatformHideWindow(backdrop_window);
	}
}

void MCScreenDC::enablebackdrop(bool p_hard)
{
	if (backdrop_enabled)
		return;
	
	backdrop_enabled = true;
	
	MCRectangle t_rect;
	MCPlatformGetScreenViewport(0, t_rect);
	MCPlatformSetWindowProperty(backdrop_window, kMCPlatformWindowPropertyFrameRect, kMCPlatformPropertyTypeRectangle, &t_rect);
	MCPlatformShowWindow(backdrop_window);
}

void MCScreenDC::disablebackdrop(bool p_hard)
{
	if (!backdrop_enabled)
		return;
	
	backdrop_enabled = false;
	
	enactraisewindows();
}

void MCScreenDC::configurebackdrop(const MCColor& p_colour, MCPatternRef p_pattern, MCImage *p_badge)
{
	backdrop_pattern = p_pattern;
	backdrop_colour = p_colour;
	
	alloccolor(backdrop_colour);
	
	MCPlatformInvalidateWindow(backdrop_window, nil);
}

void MCScreenDC::assignbackdrop(Window_mode p_mode, Window p_window)
{
}

//////////

bool MCScreenDC::isbackdrop(MCPlatformWindowRef p_window)
{
	return backdrop_window == p_window;
}

void MCScreenDC::redrawbackdrop(MCPlatformSurfaceRef p_surface, MCRegionRef p_region)
{
	MCGContextRef t_context;
	if (MCPlatformSurfaceLockGraphics(p_surface, p_region, t_context))
	{
		MCGraphicsContext *t_gfxcontext;
		/* UNCHECKED */ t_gfxcontext = new MCGraphicsContext(t_context);
		t_gfxcontext -> setforeground(backdrop_colour);
		if (backdrop_pattern != NULL)
			t_gfxcontext -> setfillstyle(FillTiled, backdrop_pattern, 0, 0);
		else
			t_gfxcontext -> setfillstyle(FillSolid, NULL, 0, 0);
		t_gfxcontext -> fillrect(MCRegionGetBoundingBox(p_region), true);
		delete t_gfxcontext;
		
		MCPlatformSurfaceUnlockGraphics(p_surface);
	}
}

void MCScreenDC::mousedowninbackdrop(uint32_t p_button, uint32_t p_count)
{
	MCdefaultstackptr -> getcard() -> message_with_args(MCM_mouse_down_in_backdrop, p_button + 1);
}

void MCScreenDC::mouseupinbackdrop(uint32_t p_button, uint32_t p_count)
{
	MCdefaultstackptr -> getcard() -> message_with_args(MCM_mouse_up_in_backdrop, p_button + 1);
}

void MCScreenDC::mousereleaseinbackdrop(uint32_t p_button)
{
}

////////////////////////////////////////////////////////////////////////////////

void MCScreenDC::grabpointer(Window w)
{
	MCPlatformGrabPointer(w);
}

void MCScreenDC::ungrabpointer()
{
	MCPlatformUngrabPointer();
}

void MCScreenDC::device_querymouse(int2 &x, int2 &y)
{
	MCPoint t_location;
	MCPlatformGetMousePosition(t_location);
	x = t_location . x;
	y = t_location . y;
}

void MCScreenDC::device_setmouse(int2 x, int2 y)
{
	MCPoint t_location;
	t_location . x = x;
	t_location . y = y;
	MCPlatformSetMousePosition(t_location);
}

MCStack *MCScreenDC::device_getstackatpoint(int32_t x, int32_t y)
{
	MCPlatformWindowRef t_window;
	MCPlatformGetWindowAtPoint(MCPointMake(x, y), t_window);
	if (t_window == nil)
		return nil;
	
	return MCdispatcher -> findstackd(t_window);
}

////////////////////////////////////////////////////////////////////////////////

MCColorTransformRef MCScreenDC::createcolortransform(const MCColorSpaceInfo& info)
{
	MCPlatformColorTransformRef t_transform;
	MCPlatformCreateColorTransform(info, t_transform);
	return t_transform;
}

void MCScreenDC::destroycolortransform(MCColorTransformRef transform)
{
	MCPlatformReleaseColorTransform((MCPlatformColorTransformRef)transform);
}

bool MCScreenDC::transformimagecolors(MCColorTransformRef transform, MCImageBitmap *image)
{
	return MCPlatformApplyColorTransform((MCPlatformColorTransformRef)transform, image);
}

////////////////////////////////////////////////////////////////////////////////

void MCScreenDC::beep()
{
	SndSetSysBeepState(sysBeepEnable | sysBeepSynchronous);
	SysBeep(beepduration / 16);
}

void MCScreenDC::getbeep(uint4 which, MCExecPoint &ep)
{
	long v;
	switch (which)
	{
		case P_BEEP_LOUDNESS:
			GetSysBeepVolume(&v);
			ep.setint(v);
			break;
		case P_BEEP_PITCH:
			ep.setint(beeppitch);
			break;
		case P_BEEP_DURATION:
			ep.setint(beepduration);
			break;
	}
}

void MCScreenDC::setbeep(uint4 which, int4 beep)
{
	switch (which)
	{
		case P_BEEP_LOUDNESS:
			SetSysBeepVolume(beep);
			break;
		case P_BEEP_PITCH:
			if (beep == -1)
				beeppitch = 1440;
			else
				beeppitch = beep;
			break;
		case P_BEEP_DURATION:
			if (beep == -1)
				beepduration = 500;
			else
				beepduration = beep;
			break;
	}
}	

////////////////////////////////////////////////////////////////////////////////

Boolean MCScreenDC::abortkey()
{
	return MCPlatformGetAbortKeyPressed();
}

uint2 MCScreenDC::querymods()
{
	if (lockmods)
		return MCmodifierstate;

	uint2 state;
	state = 0;
}

void MCScreenDC::getkeysdown(MCExecPoint &ep)
{
	// COCOA-TODO: We should be able to use CGEventSourceKeyState here - although
	//   it might be somewhat inefficient.
	ep.clear();
}

Boolean MCScreenDC::istripleclick()
{
	return tripleclick;
}

Boolean MCScreenDC::getmouse(uint2 button, Boolean& r_abort)
{
	// COCOA-TODO: See what the impact of no delay is...
	r_abort = False;
	return MCPlatformGetMouseButtonState(button);
}

Boolean MCScreenDC::getmouseclick(uint2 p_button, Boolean& r_abort)
{
	r_abort = False;
	
	MCPoint t_location;
	bool t_clicked;
	t_clicked = MCPlatformGetMouseClick(p_button, t_location);
	
	MCPoint t_clickloc;
	t_clickloc.x = t_location . x / MCResGetDeviceScale();
	t_clickloc.y = t_location . y / MCResGetDeviceScale();
	MCscreen->setclickloc(MCmousestackptr, t_clickloc);
	
	return t_clicked;
}

Boolean MCScreenDC::wait(real8 duration, Boolean dispatch, Boolean anyevent)
{
	real8 curtime = MCS_time();
	
	if (duration < 0.0)
		duration = 0.0;
	
	real8 exittime = curtime + duration;
	
	Boolean abort = False;
	Boolean reset = False;
	Boolean done = False;
	
	MCwaitdepth++;
	
	do
	{
		// Dispatch any notify events.
		if (MCNotifyDispatch(dispatch == True) && anyevent)
			break;
		
		// Handle pending events
		real8 eventtime = exittime;
		if (handlepending(curtime, eventtime, dispatch))
		{
			if (anyevent)
				done = True;
			
			if (MCquit)
			{
				abort = True;
				break;
			}
		}
		
		// MW-2012-09-19: [[ Bug 10218 ]] Make sure we update the screen in case
		//   any engine event handling methods need us to.
		MCRedrawUpdateScreen();
		
		// Get the time now
		curtime = MCS_time();
		
		// And work out how long to sleep for.
		real8 t_sleep;
		t_sleep = 0.0;
		if (curtime >= exittime)
			done = True;
		else if (!done && eventtime > curtime)
			t_sleep = MCMin(eventtime - curtime, exittime - curtime);
		
		// Wait for t_sleep seconds and collect at most one event. If an event
		// is collected and anyevent is True, then we are done.
		if (MCPlatformWaitForEvent(t_sleep, dispatch == False) && anyevent)
			done = True;
		
		// If 'quit' has been set then we must have got a finalization request
		if (MCquit)
		{
			abort = True;
			break;
		}
	}
	while(!done);
	
	MCwaitdepth--;
	
	// MW-2012-09-19: [[ Bug 10218 ]] Make sure we update the screen in case
	//   any engine event handling methods need us to.
	MCRedrawUpdateScreen();
	
	return abort;
}

void MCScreenDC::flushevents(uint2 e)
{
	// Only really the mouseDown / mouseUp / keyDown / keyUp events make sense to
	// flush these days, the remaining event types are quite tied to Mac Classic!
	MCPlatformEventMask t_mask;
	t_mask = 0;
	if (e == FE_MOUSEDOWN)
		t_mask = kMCPlatformEventMouseDown;
	else if (e == FE_MOUSEUP)
		t_mask = kMCPlatformEventMouseUp;
	else if (e == FE_KEYDOWN)
		t_mask = kMCPlatformEventKeyDown;
	else if (e == FE_KEYUP)
		t_mask = kMCPlatformEventKeyUp;
	
	if (t_mask != nil)
		MCPlatformFlushEvents(t_mask);
}

////////////////////////////////////////////////////////////////////////////////

void MCScreenDC::clearIME(Window w)
{
}

void MCScreenDC::openIME()
{
}

void MCScreenDC::activateIME(Boolean activate)
{
}

void MCScreenDC::closeIME()
{
}

////////////////////////////////////////////////////////////////////////////////

void MCScreenDC::listprinters(MCExecPoint& ep)
{
}

// COCOA-TODO: Printer creation.
MCPrinter *MCScreenDC::createprinter(void)
{
	return MCUIDC::createprinter();
}

////////////////////////////////////////////////////////////////////////////////

static uindex_t s_clipboard_generation = 0;
static MCPasteboard *s_local_clipboard = nil;

MCSharedString *MCConvertStyledTextToUTF8(MCSharedString *p_in)
{
	MCObject *t_object;
	t_object = MCObject::unpickle(p_in, MCtemplatefield -> getstack());
	if (t_object != NULL)
	{
		MCParagraph *t_paragraphs;
		t_paragraphs = ((MCStyledText *)t_object) -> getparagraphs();

		MCExecPoint ep(NULL, NULL, NULL);

		// MW-2012-02-21: [[ FieldExport ]] Use the new plain text export method.
		if (t_paragraphs != NULL)
		{
			MCtemplatefield -> exportasplaintext(ep, t_paragraphs, 0, INT32_MAX, true);
			ep . utf16toutf8();
		}

		delete t_object;

		return MCSharedString::Create(ep . getsvalue());
	}
	return NULL;
}

MCSharedString *MCConvertUnicodeTextToUTF8(MCSharedString* p_in)
{
	MCExecPoint ep(NULL, NULL, NULL);
	ep . setsvalue(p_in -> Get());
	ep . utf16toutf8();
	return MCSharedString::Create(ep . getsvalue());
}

MCSharedString *MCConvertTextToUTF8(MCSharedString *p_in)
{
	MCExecPoint ep(NULL, NULL, NULL);
	ep . setsvalue(p_in -> Get());
	ep . nativetoutf8();
	return MCSharedString::Create(ep . getsvalue());
}

MCSharedString *MCConvertIdentity(MCSharedString *p_in)
{
	p_in -> Retain();
	return p_in;
}

static struct { MCTransferType type; MCPlatformPasteboardFlavor flavor; MCSharedString* (*convert)(MCSharedString* in); } s_pasteboard_fetchers[] =
{
	{ TRANSFER_TYPE_STYLED_TEXT, kMCPlatformPasteboardFlavorRTF, MCConvertStyledTextToRTF },
	{ TRANSFER_TYPE_STYLED_TEXT, kMCPlatformPasteboardFlavorUTF8, MCConvertStyledTextToUTF8 },
	{ TRANSFER_TYPE_UNICODE_TEXT, kMCPlatformPasteboardFlavorUTF8, MCConvertUnicodeTextToUTF8 },
	{ TRANSFER_TYPE_TEXT, kMCPlatformPasteboardFlavorUTF8, MCConvertTextToUTF8 },
	{ TRANSFER_TYPE_IMAGE, kMCPlatformPasteboardFlavorPNG, MCConvertIdentity },
	{ TRANSFER_TYPE_IMAGE, kMCPlatformPasteboardFlavorGIF, MCConvertIdentity },
	{ TRANSFER_TYPE_IMAGE, kMCPlatformPasteboardFlavorJPEG, MCConvertIdentity },
	{ TRANSFER_TYPE_FILES, kMCPlatformPasteboardFlavorFiles, MCConvertIdentity },
	{ TRANSFER_TYPE_OBJECTS, kMCPlatformPasteboardFlavorObjects, MCConvertIdentity },
};

static bool fetch_clipboard(MCPlatformPasteboardFlavor p_flavor, void*& r_data, size_t& r_data_size)
{
	if (s_local_clipboard == nil)
		return false;
		
	MCTransferType *t_types;
	uindex_t t_type_count;
	if (!s_local_clipboard -> Query(t_types, t_type_count))
		return false;

	for(uindex_t i = 0; i < sizeof(s_pasteboard_fetchers) / sizeof(s_pasteboard_fetchers[0]); i++)
		for(uindex_t j = 0; j < t_type_count; j++)
			if (s_pasteboard_fetchers[i] . type == t_types[j] && s_pasteboard_fetchers[i] . flavor == p_flavor)
			{
				MCSharedString *t_data;
				if (!s_local_clipboard -> Fetch(t_types[j], t_data))
					return false;
					
				bool t_success;
				t_success = false;
	
				MCSharedString *t_new_data;
				t_new_data = s_pasteboard_fetchers[i] . convert(t_data);
				if (t_new_data != nil)
				{
					if (MCMemoryAllocateCopy(t_new_data -> GetBuffer(), t_new_data -> GetLength(), r_data))
					{
						r_data_size = t_new_data -> GetLength();
						t_success = true;
					}
					t_new_data -> Release();
				}
				
				t_data -> Release();
				
				return t_success;
			}
	
	return true;
}

void MCScreenDC::flushclipboard(void)
{
}

bool MCScreenDC::ownsclipboard(void)
{
	MCPlatformPasteboardRef t_pasteboard;
	MCPlatformGetClipboard(t_pasteboard);
	if (MCPlatformPasteboardGetGeneration(t_pasteboard) == s_clipboard_generation)
		return true;
		
	if (s_local_clipboard != nil)
	{
		s_local_clipboard -> Release();
		s_local_clipboard = nil;
	}
		
	return false;
}

bool MCScreenDC::setclipboard(MCPasteboard *p_pasteboard)
{
	MCPlatformPasteboardRef t_clipboard;
	MCPlatformGetClipboard(t_clipboard);
	
	MCPlatformPasteboardClear(t_clipboard);
	if (s_local_clipboard != nil)
	{
		s_local_clipboard -> Release();
		s_local_clipboard = nil;
	}
	
	MCTransferType *t_types;
	uindex_t t_type_count;
	if (!p_pasteboard -> Query(t_types, t_type_count))
		return false;
		
	for(uindex_t i = 0; i < t_type_count; i++)
	{
		MCPlatformPasteboardFlavor t_flavors[2];
		uindex_t t_flavor_count;
		t_flavor_count = 0;
		
		switch(t_types[i])
		{
		case TRANSFER_TYPE_TEXT:
		case TRANSFER_TYPE_UNICODE_TEXT:
			t_flavors[t_flavor_count++] = kMCPlatformPasteboardFlavorUTF8;
			break;
		case TRANSFER_TYPE_STYLED_TEXT:
			t_flavors[t_flavor_count++] = kMCPlatformPasteboardFlavorRTF;
			t_flavors[t_flavor_count++] = kMCPlatformPasteboardFlavorUTF8;
		break;
		case TRANSFER_TYPE_IMAGE:
		{
			MCSharedString *t_data;
			if (p_pasteboard -> Fetch(TRANSFER_TYPE_IMAGE, t_data))
			{
				if (MCFormatImageIsPNG(t_data))
					t_flavors[t_flavor_count++] = kMCPlatformPasteboardFlavorPNG;
				if (MCFormatImageIsGIF(t_data))
					t_flavors[t_flavor_count++] = kMCPlatformPasteboardFlavorGIF;
				if (MCFormatImageIsJPEG(t_data))
					t_flavors[t_flavor_count++] = kMCPlatformPasteboardFlavorJPEG;
			}
		}
		break;
		case TRANSFER_TYPE_FILES:
			t_flavors[t_flavor_count++] = kMCPlatformPasteboardFlavorFiles;
			break;
		case TRANSFER_TYPE_OBJECTS:
			t_flavors[t_flavor_count++] = kMCPlatformPasteboardFlavorObjects;
			break;
		case TRANSFER_TYPE_PRIVATE:
			break;
		}
		
		if (t_flavor_count != 0)
			MCPlatformPasteboardStore(t_clipboard, t_flavors, t_flavor_count, (void *)fetch_clipboard);
	}
	
	s_local_clipboard = p_pasteboard;
	s_local_clipboard -> Retain();
	s_clipboard_generation = MCPlatformPasteboardGetGeneration(t_clipboard);
	
	return true;
}

MCPasteboard *MCScreenDC::getclipboard(void)
{
	MCPlatformPasteboardRef t_pasteboard;
	MCPlatformGetClipboard(t_pasteboard);
	
	MCPasteboard *t_clipboard;
	t_clipboard = new MCSystemPasteboard(t_pasteboard);
	
	MCPlatformPasteboardRelease(t_pasteboard);
	
	return t_clipboard;
}

////////////////////////////////////////////////////////////////////////////////

// TD-2013-07-01: [[ DynamicFonts ]]
bool MCScreenDC::loadfont(const char *p_path, bool p_globally, void*& r_loaded_font_handle)
{
	MCExecPoint ep;
	ep . setsvalue(p_path);
	ep . nativetoutf8();
	return MCPlatformLoadFont(ep . getcstring(), p_globally, (MCPlatformLoadedFontRef&)r_loaded_font_handle);
}

bool MCScreenDC::unloadfont(const char *p_path, bool p_globally, void *p_loaded_font_handle)
{
	MCExecPoint ep;
	ep . setsvalue(p_path);
	ep . nativetoutf8();
	return MCPlatformUnloadFont(ep . getcstring(), p_globally, (MCPlatformLoadedFontRef)p_loaded_font_handle);
}

////////////////////////////////////////////////////////////////////////////////

MCScriptEnvironment *MCScreenDC::createscriptenvironment(const char *p_language)
{
	return nil;
}

////////////////////////////////////////////////////////////////////////////////

MCDragAction MCScreenDC::dodragdrop(Window w, MCPasteboard *p_pasteboard, MCDragActionSet p_allowed_actions, MCImage *p_image, const MCPoint* p_image_offset)
{
	MCPlatformDragOperation t_op;
	MCPlatformDoDragDrop(w, p_allowed_actions, nil, nil, t_op);
	
	MCDragAction t_action;
	switch(t_op)
	{
		case kMCPlatformDragOperationNone:
			t_action = DRAG_ACTION_NONE;
			break;
		case kMCPlatformDragOperationCopy:
			t_action = DRAG_ACTION_COPY;
			break;
		case kMCPlatformDragOperationLink:
			t_action = DRAG_ACTION_LINK;
			break;
		case kMCPlatformDragOperationMove:
			t_action = DRAG_ACTION_MOVE;
			break;
	}
	return t_action;
}

////////////////////////////////////////////////////////////////////////////////

MCImageBitmap *MCScreenDC::snapshot(MCRectangle &p_rect, MCGFloat p_scale_factor, uint4 p_window, const char *p_display_name)
{
	MCImageBitmap *t_bitmap;
	if (p_window == 0 && (p_rect . width == 0 || p_rect . height == 0))
		MCPlatformScreenSnapshotOfUserArea(t_bitmap);
	else if (p_window != 0)
		MCPlatformScreenSnapshotOfWindow(p_window, t_bitmap);
	else
		MCPlatformScreenSnapshot(p_rect, t_bitmap);
}

////////////////////////////////////////////////////////////////////////////////

double MCMacGetAnimationStartTime(void)
{
	return 0.0;
}

double MCMacGetAnimationCurrentTime(void)
{
	return 0.0;
}

WindowPtr MCMacGetInvisibleWindow(void)
{
	return nil;
}

void MCMacBreakWait(void)
{
	MCPlatformBreakWait();
}

////////////////////////////////////////////////////////////////////////////////

// IM-2013-08-01: [[ ResIndependence ]] OSX implementation currently returns 1.0
MCGFloat MCResGetDeviceScale(void)
{
	// COCOA-TODO: Integrate with platform
	return 1.0;
}

////////////////////////////////////////////////////////////////////////////////

MCUIDC *MCCreateScreenDC(void)
{
	return new MCScreenDC;
}

////////////////////////////////////////////////////////////////////////////////
