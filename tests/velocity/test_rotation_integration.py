"""Lifecycle/WSI integration guards; physical Android rotation remains pending."""
import unittest
from pathlib import Path
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[2]
JAVA=ROOT/'android/app/src/main/java/com/nfsu2/androidevolved'
class RotationIntegration(unittest.TestCase):
    def test_game_activity_handles_rotation_without_recreation(self):
        app=ET.parse(ROOT/'android/app/src/main/AndroidManifest.xml').getroot().find('application')
        attrs='{http://schemas.android.com/apk/res/android}'
        game=next(a for a in app.findall('activity') if a.attrib[attrs+'name']=='.GameActivity')
        self.assertEqual(game.attrib[attrs+'screenOrientation'],'userLandscape')
        self.assertTrue({'orientation','screenSize','screenLayout','smallestScreenSize','density'}<=set(game.attrib[attrs+'configChanges'].split('|')))
        source=(JAVA/'GameActivity.java').read_text()
        self.assertIn('DisplayManager.DisplayListener',source)
        self.assertIn('if(rotation!=lastRotation)',source)
        section=source.split('public void onConfigurationChanged',1)[1].split('@Override public void onWindowFocusChanged',1)[0]
        self.assertNotIn('recreate()',section)
        self.assertIn('refreshDisplayLayout',section)
    def test_touch_and_hud_share_inset_update(self):
        source=(JAVA/'GameActivity.java').read_text()
        self.assertIn('nativeSurfaceLayout(width,height,left,top)',source)
        self.assertIn('touchControls.setSafeArea(left,top,right,bottom',source)
        controls=(JAVA/'RacingControlsView.java').read_text()
        self.assertIn('releaseAll(); controls.clear()',controls)
        self.assertIn('c.area.offset(dx,dy)',controls)
        self.assertNotIn('SYSTEM_ALERT_WINDOW',(ROOT/'android/app/src/main/AndroidManifest.xml').read_text())
    def test_wsi_retains_generation_on_same_window(self):
        source=(ROOT/'ports/dxvk_android/android_wsi.cpp').read_text()
        self.assertIn('if(window==currentWindow)return;',source)
        commands=(ROOT/'android/app/src/main/cpp/main.cpp').read_text().split('static void onCommand',1)[1].split('static int32_t onInput',1)[0]
        resized=commands.split('if(command==APP_CMD_INIT_WINDOW||command==APP_CMD_WINDOW_RESIZED)',1)[1]
        self.assertNotIn('setD3D9HostWindow(nullptr)',resized)
        self.assertNotIn('guestWorker.start',resized)
        self.assertIn('dxvkAndroidResetHudClock()',commands)
if __name__=='__main__':unittest.main()
