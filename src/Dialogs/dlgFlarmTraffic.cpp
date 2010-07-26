/*
  Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009

	M Roberts (original release)
	Robin Birch <robinb@ruffnready.co.uk>
	Samuel Gisiger <samuel.gisiger@triadis.ch>
	Jeff Goodenough <jeff@enborne.f2s.com>
	Alastair Harrison <aharrison@magic.force9.co.uk>
	Scott Penrose <scottp@dd.com.au>
	John Wharington <jwharington@gmail.com>
	Lars H <lars_hn@hotmail.com>
	Rob Dunning <rob@raspberryridgesheepfarm.com>
	Russell King <rmk@arm.linux.org.uk>
	Paolo Ventafridda <coolwind@email.it>
	Tobias Lohner <tobias@lohner-net.de>
	Mirek Jezek <mjezek@ipplc.cz>
	Max Kellermann <max@duempel.org>
	Tobias Bieniek <tobias.bieniek@gmx.de>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

/**
 * @file
 * The FLARM Traffic dialog displaying a radar screen with the moving
 * FLARM targets in track up orientation. The target can be selected and basic
 * information is given on the selected target. When a warning/alarm is present
 * the target with the highest alarm level is automatically selected and
 * highlighted in orange or red (depending on the level)
 */

#include "Dialogs/Internal.hpp"
#include "Screen/Fonts.hpp"
#include "Screen/Layout.hpp"
#include "Screen/CheckBox.hpp"
#include "MainWindow.hpp"
#include "Profile.hpp"
#include "Compiler.h"
#include "Gauge/FlarmTrafficWindow.hpp"

enum {
  ID_AUTO_ZOOM = 100,
};

/**
 * A Window which renders FLARM traffic, with user interaction.
 */
class FlarmTrafficControl : public FlarmTrafficWindow {
protected:
  bool enable_auto_zoom;
  unsigned zoom;
  Font hfInfoValues, hfInfoLabels, hfCallSign;

public:
  FlarmTrafficControl()
    :FlarmTrafficWindow(Layout::Scale(10)),
     enable_auto_zoom(true),
     zoom(2) {}

protected:
  void CalcAutoZoom();

public:
  void Update(Angle new_direction, const FLARM_STATE &new_data,
              const SETTINGS_TEAMCODE &new_settings);

  bool GetAutoZoom() const {
    return enable_auto_zoom;
  }

  static unsigned GetZoomDistance(unsigned zoom);

  void SetZoom(unsigned _zoom) {
    zoom = _zoom;
    SetDistance(fixed(GetZoomDistance(_zoom)));
  }

  void SetAutoZoom(bool enabled);

  void ZoomOut();
  void ZoomIn();

protected:
  void PaintTrafficInfo(Canvas &canvas) const;

protected:
  virtual bool on_create();
  virtual void on_paint(Canvas &canvas);
  virtual bool on_mouse_down(int x, int y);
};

static WndForm *wf = NULL;
static FlarmTrafficControl *wdf;
static CheckBox *auto_zoom;

bool
FlarmTrafficControl::on_create()
{
  FlarmTrafficWindow::on_create();

  hfInfoLabels.set(Fonts::GetStandardFontFace(), Layout::FastScale(10), true);
  hfInfoValues.set(Fonts::GetStandardFontFace(), Layout::FastScale(20));
  hfCallSign.set(Fonts::GetStandardFontFace(), Layout::FastScale(28), true);

  Profile::Get(szProfileFlarmSideData, side_display_type);
  Profile::Get(szProfileFlarmAutoZoom, enable_auto_zoom);

  return true;
}

unsigned
FlarmTrafficControl::GetZoomDistance(unsigned zoom)
{
  switch (zoom) {
  case 0:
    return 500;
  case 1:
    return 1000;
  case 3:
    return 5000;
  case 4:
    return 10000;
  case 2:
  default:
    return 2000;
  }
}

void
FlarmTrafficControl::SetAutoZoom(bool enabled)
{
  enable_auto_zoom = enabled;
  Profile::Set(szProfileFlarmAutoZoom, enabled);
  auto_zoom->set_checked(enabled);
}

void
FlarmTrafficControl::CalcAutoZoom()
{
  bool warning_mode = WarningMode();
  fixed zoom_dist = fixed_zero;

  for (unsigned i = 0; i < FLARM_STATE::FLARM_MAX_TRAFFIC; i++) {
    if (!data.FLARM_Traffic[i].defined())
      continue;

    if (warning_mode && !data.FLARM_Traffic[i].HasAlarm())
      continue;

    fixed dist = data.FLARM_Traffic[i].RelativeNorth
                * data.FLARM_Traffic[i].RelativeNorth
                + data.FLARM_Traffic[i].RelativeEast
                * data.FLARM_Traffic[i].RelativeEast;

    zoom_dist = max(dist, zoom_dist);
  }

  for (unsigned i = 0; i <= 4; i++) {
    if (i == 4 ||
        fixed(GetZoomDistance(i) * GetZoomDistance(i)) >= zoom_dist) {
      SetZoom(i);
      break;
    }
  }
}

void
FlarmTrafficControl::Update(Angle new_direction, const FLARM_STATE &new_data,
                            const SETTINGS_TEAMCODE &new_settings)
{
  FlarmTrafficWindow::Update(new_direction, new_data, new_settings);

  if (enable_auto_zoom || WarningMode())
    CalcAutoZoom();
}

/**
 * Zoom out one step
 */
void
FlarmTrafficControl::ZoomOut()
{
  if (WarningMode())
    return;

  if (zoom < 4)
    SetZoom(zoom + 1);

  SetAutoZoom(false);
}

/**
 * Zoom in one step
 */
void
FlarmTrafficControl::ZoomIn()
{
  if (WarningMode())
    return;

  if (zoom > 0)
    SetZoom(zoom - 1);

  SetAutoZoom(false);
}

/**
 * Paints the basic info for the selected target on the given canvas
 * @param canvas The canvas to paint on
 */
void
FlarmTrafficControl::PaintTrafficInfo(Canvas &canvas) const
{
  // Don't paint numbers if no plane selected
  if (selection == -1)
    return;

  assert(data.FLARM_Traffic[selection].defined());

  // Temporary string
  TCHAR tmp[20];
  // Temporary string size
  SIZE sz;
  // Shortcut to the selected traffic
  FLARM_TRAFFIC traffic;
  if (WarningMode())
    traffic = data.FLARM_Traffic[warning];
  else
    traffic = data.FLARM_Traffic[selection];

  assert(traffic.defined());

  RECT rc;
  rc.left = padding;
  rc.top = padding;
  rc.right = canvas.get_width() - padding;
  rc.bottom = canvas.get_height() - padding;

  // Set the text color and background
  switch (traffic.AlarmLevel) {
  case 1:
    canvas.set_text_color(hcWarning);
    break;
  case 2:
  case 3:
    canvas.set_text_color(hcAlarm);
    break;
  case 4:
  case 0:
  default:
    canvas.set_text_color(hcStandard);
    break;
  }
  canvas.background_transparent();

  // Climb Rate
  if (!WarningMode()) {
#ifdef FLARM_AVERAGE
    Units::FormatUserVSpeed(traffic.Average30s, tmp, 20);
#else
    Units::FormatUserVSpeed(traffic.ClimbRate, tmp, 20);
#endif
    canvas.select(hfInfoValues);
    sz = canvas.text_size(tmp);
    canvas.text(rc.right - sz.cx, rc.top + hfInfoLabels.get_height(), tmp);

    canvas.select(hfInfoLabels);
    sz = canvas.text_size(_T("Vario:"));
    canvas.text(rc.right - sz.cx, rc.top, _T("Vario:"));
  }

  // Distance
  Units::FormatUserDistance(hypot(traffic.RelativeEast, traffic.RelativeNorth),
                            tmp, 20);
  canvas.select(hfInfoValues);
  sz = canvas.text_size(tmp);
  canvas.text(rc.left, rc.bottom - sz.cy, tmp);

  canvas.select(hfInfoLabels);
  canvas.text(rc.left,
              rc.bottom - hfInfoValues.get_height() - hfInfoLabels.get_height(),
              _T("Distance:"));

  // Relative Height
  Units::FormatUserArrival(traffic.RelativeAltitude, tmp, 20);
  canvas.select(hfInfoValues);
  sz = canvas.text_size(tmp);
  canvas.text(rc.right - sz.cx, rc.bottom - sz.cy, tmp);

  canvas.select(hfInfoLabels);
  sz = canvas.text_size(_T("Rel. Alt.:"));
  canvas.text(rc.right - sz.cx,
              rc.bottom - hfInfoValues.get_height() - hfInfoLabels.get_height(),
              _T("Rel. Alt.:"));

  // ID / Name
  if (traffic.HasName()) {
    canvas.select(hfCallSign);
    if (!traffic.HasAlarm()) {
      if (settings.TeamFlarmTracking &&
          traffic.ID == settings.TeamFlarmIdTarget)
        canvas.set_text_color(hcTeam);
      else
        canvas.set_text_color(hcSelection);
    }
    _tcscpy(tmp, traffic.Name);
  } else {
    traffic.ID.format(tmp);
  }
  canvas.text(rc.left, rc.top, tmp);
}

void
FlarmTrafficControl::on_paint(Canvas &canvas)
{
  FlarmTrafficWindow::on_paint(canvas);

  PaintTrafficInfo(canvas);
}

/**
 * This event handler is called when the "Details" button is pressed
 */
static void
OnDetailsClicked(gcc_unused WndButton &button)
{

  // If warning is displayed -> prevent from opening details dialog
  if (wdf->WarningMode())
    return;

  // Don't open the details dialog if no plane selected
  const FLARM_TRAFFIC *traffic = wdf->GetTarget();
  if (traffic == NULL)
    return;

  // Show the details dialog
  dlgFlarmTrafficDetailsShowModal(traffic->ID);
}

/**
 * This event handler is called when the "ZoomIn (+)" button is pressed
 */
static void
OnZoomInClicked(gcc_unused WndButton &button)
{
  wdf->ZoomIn();
}

/**
 * This event handler is called when the "ZoomOut (-)" button is pressed
 */
static void
OnZoomOutClicked(gcc_unused WndButton &button)
{
  wdf->ZoomOut();
}

/**
 * This event handler is called when the "Prev (<)" button is pressed
 */
static void
OnPrevClicked(gcc_unused WndButton &button)
{
  // If warning is displayed -> prevent selector movement
  if (wdf->WarningMode())
    return;

  wdf->PrevTarget();
}

/**
 * This event handler is called when the "Next (>)" button is pressed
 */
static void
OnNextClicked(gcc_unused WndButton &button)
{
  // If warning is displayed -> prevent selector movement
  if (wdf->WarningMode())
    return;

  wdf->NextTarget();
}

/**
 * This event handler is called when the "Close" button is pressed
 */
static void
OnCloseClicked(gcc_unused WndButton &button)
{
  wf->SetModalResult(mrOK);
}

/**
 * This event handler is called when the "Avg/Alt" button is pressed
 */
static void
OnSwitchDataClicked(gcc_unused WndButton &button)
{
  wdf->side_display_type++;
  if (wdf->side_display_type > 2)
    wdf->side_display_type = 1;

  Profile::Set(szProfileFlarmSideData, wdf->side_display_type);
}

/**
 * This event handler is called when the "AutoZoom" button is pressed
 */
static bool
OnCommand(unsigned id)
{
  switch (id) {
  case ID_AUTO_ZOOM:
    wdf->SetAutoZoom(auto_zoom->get_checked());
    return true;

  default:
    return false;
  }
}

/**
 * This event handler is called when a key is pressed
 * @param key_code The key code of the pressed key
 * @return True if the event was handled, False otherwise
 */
static bool
FormKeyDown(WindowControl *Sender, unsigned key_code)
{
  (void)Sender;

  switch (key_code) {
  case VK_UP:
    if (!has_pointer())
      break;

    wdf->ZoomIn();
    return true;
  case VK_DOWN:
    if (!has_pointer())
      break;

    wdf->ZoomOut();
    return true;
  case VK_LEFT:
  case '6':
    wdf->PrevTarget();
    return true;
  case VK_RIGHT:
  case '7':
    wdf->NextTarget();
    return true;
  }

  return false;
}

/**
 * This event handler is called when the timer is activated and triggers the
 * repainting of the radar
 */
static int
OnTimerNotify(WindowControl * Sender)
{
  (void)Sender;
  wdf->Update(XCSoarInterface::Basic().TrackBearing,
              XCSoarInterface::Basic().flarm,
              XCSoarInterface::SettingsComputer());
  return 0;
}

bool
FlarmTrafficControl::on_mouse_down(int x, int y)
{
  if (!WarningMode())
    SelectNearTarget(x, y, Layout::Scale(15));

  return true;
}

static Window *
OnCreateFlarmTrafficControl(ContainerWindow &parent, int left, int top,
                            unsigned width, unsigned height,
                            const WindowStyle style)
{
  wdf = new FlarmTrafficControl();
  wdf->set(parent, left, top, width, height, style);

  return wdf;
}

static Window *
OnCreateAutoZoom(ContainerWindow &parent, int left, int top,
                 unsigned width, unsigned height,
                 const WindowStyle _style)
{
  CheckBoxStyle style(_style);
  style.tab_stop();
  auto_zoom = new CheckBox();
  auto_zoom->set(parent, gettext(_T("A. Zoom")), ID_AUTO_ZOOM,
                 left, top, width, height, style);
  auto_zoom->set_font(Fonts::Map);
  return auto_zoom;
}

static CallBackTableEntry_t CallBackTable[] = {
  DeclareCallBackEntry(OnCreateFlarmTrafficControl),
  DeclareCallBackEntry(OnCreateAutoZoom),
  DeclareCallBackEntry(OnTimerNotify),
  DeclareCallBackEntry(NULL)
};

/**
 * The function opens the FLARM Traffic dialog
 */
void
dlgFlarmTrafficShowModal()
{
  // Load dialog from XML
  if (Layout::landscape)
    wf = dlgLoadFromXML(CallBackTable,
        XCSoarInterface::main_window, _T("IDR_XML_FLARMTRAFFIC_L"));
  else
    wf = dlgLoadFromXML(CallBackTable,
        XCSoarInterface::main_window, _T("IDR_XML_FLARMTRAFFIC"));

  if (!wf)
    return;

  // Set dialog events
  wf->SetKeyDownNotify(FormKeyDown);
  wf->SetTimerNotify(OnTimerNotify);

  wf->SetCommandCallback(OnCommand);

  // Set button events
  ((WndButton *)wf->FindByName(_T("cmdDetails")))->
      SetOnClickNotify(OnDetailsClicked);
  ((WndButton *)wf->FindByName(_T("cmdZoomIn")))->
      SetOnClickNotify(OnZoomInClicked);
  ((WndButton *)wf->FindByName(_T("cmdZoomOut")))->
      SetOnClickNotify(OnZoomOutClicked);
  ((WndButton *)wf->FindByName(_T("cmdPrev")))->
      SetOnClickNotify(OnPrevClicked);
  ((WndButton *)wf->FindByName(_T("cmdNext")))->
      SetOnClickNotify(OnNextClicked);
  ((WndButton *)wf->FindByName(_T("cmdClose")))->
      SetOnClickNotify(OnCloseClicked);
  ((WndButton *)wf->FindByName(_T("cmdSwitchData")))->
      SetOnClickNotify(OnSwitchDataClicked);

  // Update Radar and Selection for the first time
  wdf->Update(XCSoarInterface::Basic().TrackBearing,
              XCSoarInterface::Basic().flarm,
              XCSoarInterface::SettingsComputer());

  // Get the last chosen Side Data configuration
  auto_zoom->set_checked(wdf->GetAutoZoom());

  // Show the dialog
  wf->ShowModal();

  // After dialog closed -> delete it
  delete wf;
}
