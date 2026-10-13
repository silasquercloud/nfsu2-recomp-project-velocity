"""CSV compatibility/parser checks, not measured game benchmarks."""
import importlib.util,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('reports',ROOT/'scripts/compare_velocity_reports.py');reports=importlib.util.module_from_spec(spec);spec.loader.exec_module(reports)
class Comparison(unittest.TestCase):
    def test_comment_metadata_and_missing_optional_metrics(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)/'fixture.csv'
            p.write_text('# d3d9_op=kind,method,calls,samples,sampled_host_ns,sampled_max_host_ns\n# d3d9_op=2,82,128,17,17000000,3000000\nframe,wall_ns,cpu_process_ns,draw_calls\n1,10000000,5000000,4\n2,30000000,10000000,8\n')
            value=reports.summarize(p)
            self.assertEqual(value['fps_average'],50)
            self.assertAlmostEqual(value['fps_1_percent_low'],100/3)
            self.assertEqual(value['p50_ms'],10);self.assertEqual(value['p99_ms'],30)
            self.assertIsNone(value['gpu_execution_ms'])
            self.assertEqual(value['d3d9_operations_by_estimated_cost'][0]['calls'],128)
            self.assertNotIn('lock_scratch_reuse_mean',value)
    def test_no_capture_is_not_a_benchmark(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)/'empty.csv';p.write_text('# no captured frames\n')
            with self.assertRaises(ValueError):reports.summarize(p)
if __name__=='__main__':unittest.main()
