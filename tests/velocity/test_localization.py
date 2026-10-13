"""Resource completeness and fallback guards; not an Android UI/device test."""
from pathlib import Path
import re
import unittest
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]
RES=ROOT/'android/app/src/main/res'
JAVA=ROOT/'android/app/src/main/java/com/nfsu2/androidevolved'

def strings(folder):
    elements=ET.parse(RES/folder/'strings.xml').getroot().findall('string')
    result={item.attrib['name']:''.join(item.itertext()) for item in elements}
    if len(result)!=len(elements):raise AssertionError('Duplicate string identifiers')
    return result

class Localization(unittest.TestCase):
    def test_pt_br_covers_every_default_string(self):
        default,portuguese=strings('values'),strings('values-pt-rBR')
        self.assertEqual(default.keys(),portuguese.keys())
        self.assertGreaterEqual(len(default),200)
        for key in default:
            self.assertTrue(portuguese[key].strip('"').strip(),key)
            # Preserve any formatter placeholders if one is introduced later.
            pattern=r'%(?:\d+\$)?[sdfox]'
            self.assertEqual(re.findall(pattern,default[key]),re.findall(pattern,portuguese[key]),key)

    def test_critical_labels_and_default_fallback(self):
        default,pt=strings('values'),strings('values-pt-rBR')
        self.assertIn('JOGAR',pt['play'])
        self.assertIn('Desempenho',pt['velocity_section'])
        self.assertIn('Português (Brasil)',pt['locale_portuguese'])
        self.assertIn('PLAY',default['play'])
        # Unsupported locales have a complete unqualified English resource set.
        for source in JAVA.glob('*.java'):
            for key in re.findall(r'R\.string\.(\w+)',source.read_text()):self.assertIn(key,default,source.name)
        self.assertIn('android:label="@string/app_name"',(ROOT/'android/app/src/main/AndroidManifest.xml').read_text())

    def test_manual_locale_is_applied_in_every_activity_and_separate_from_game(self):
        policy=(JAVA/'UiText.java').read_text()
        self.assertIn('config.setLocales',policy)
        self.assertIn('Locale.forLanguageTag(tag)',policy)
        self.assertIn('return context;',policy)
        for activity in ('LauncherActivity','GameActivity','GpuProbeActivity'):
            self.assertIn('super.attachBaseContext(UiText.wrap(base))',(JAVA/(activity+'.java')).read_text())
        self.assertIn('launcher_locale',policy)
        self.assertIn('putString(UiText.PREF,tags[which])',(JAVA/'LauncherActivity.java').read_text())
        self.assertIn('getString(LANGUAGE, "Spanish")',(JAVA/'GameOptions.java').read_text())

    def test_translatable_widget_text_is_not_hardcoded(self):
        # Product name and diagnostic prefix are intentionally proper names.
        for source in JAVA.glob('*.java'):
            for match in re.findall(r'\.set(?:Title|Hint|Message|Error|ContentDescription|PositiveButton|NegativeButton|NeutralButton)\("([^"\n]+)"',source.read_text()):
                self.fail(f'Hardcoded widget text in {source.name}: {match}')
        self.assertNotIn('literalLabels',(JAVA/'GameOptions.java').read_text())
        self.assertNotRegex((JAVA/'GameOptions.java').read_text(),r'new Option\(\w+,\s*"')
        self.assertNotIn('option.labels,',(JAVA/'GameActivity.java').read_text())
        self.assertNotIn('option.labels,',(JAVA/'LauncherActivity.java').read_text())

if __name__=='__main__':unittest.main()
