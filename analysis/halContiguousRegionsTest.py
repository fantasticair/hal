#!/usr/bin/env python

import unittest
from hal.analysis.halContiguousRegions import ContiguousRegions

class ContiguousRegionsTestCase(unittest.TestCase):
    def setUp(self):
        self.contiguousRegions = ContiguousRegions('/dev/null', 'src',
                                                   'target', 100, '/tmp/',
                                                   10000, False, 0.25)
        self.maxDiff = None

    def test_mergeBlocks_noDups(self):
        blocks = { 'seq1': [((0, 20, '+'), (20, 40, '+')),
                            ((80, 100, '+'), (0, 20, '-')),
                            ((20, 40, '+'), (50, 70, '+'))]
                 }
        merged = { 'seq1': [((0, 20, '+'), [(20, 40, '+')]),
                            ((20, 40, '+'), [(50, 70, '+')]),
                            ((80, 100, '+'), [(0, 20, '-')])]
                 }
        self.assertEqual(self.contiguousRegions.mergeBlocks(blocks), merged)

    def test_mergeBlocks_simpleDuped(self):
        blocks = {
                   # overlapping in the middle of a block
                   'seq1': [((0, 20, '+'), (20, 40, '+')),
                            ((10, 15, '+'), (0, 5, '+')),
                            ((20, 40, '+'), (50, 70, '+'))],
                   # overlapping and going past the end of a block
                   'seq2': [((0, 20, '+'), (20, 40, '+')),
                            ((10, 30, '+'), (0, 20, '+'))]
                 }
        merged = { 'seq1': [((0, 10, '+'), [(20, 30, '+')]),
                            ((10, 15, '+'), [(30, 35, '+'), (0, 5, '+')]),
                            ((15, 20, '+'), [(35, 40, '+')]),
                            ((20, 40, '+'), [(50, 70, '+')])],
                   'seq2': [((0, 10, '+'), [(20, 30, '+')]),
                            ((10, 20, '+'), [(30, 40, '+'), (0, 10, '+')]),
                            ((20, 30, '+'), [(10, 20, '+')])]
                 }
        self.assertEqual(self.contiguousRegions.mergeBlocks(blocks), merged)

    def test_mergeBlocks_negativeStrandDuped(self):
        blocks = {
                   # overlapping in the middle of a block
                   'seq1': [((0, 20, '+'), (20, 40, '-')),
                            ((10, 15, '+'), (0, 5, '-')),
                            ((20, 40, '+'), (50, 70, '-'))],
                   # overlapping and going past the end of a block
                   'seq2': [((0, 20, '+'), (20, 40, '-')),
                            ((10, 30, '+'), (0, 20, '-'))]
                 }
        merged = { 'seq1': [((0, 10, '+'), [(30, 40, '-')]),
                            ((10, 15, '+'), [(25, 30, '-'), (0, 5, '-')]),
                            ((15, 20, '+'), [(20, 25, '-')]),
                            ((20, 40, '+'), [(50, 70, '-')])],
                   'seq2': [((0, 10, '+'), [(30, 40, '-')]),
                            ((10, 20, '+'), [(20, 30, '-'), (10, 20, '-')]),
                            ((20, 30, '+'), [(0, 10, '-')])]
                 }
        self.assertEqual(self.contiguousRegions.mergeBlocks(blocks), merged)

    def test_mergeBlocks_mixedStrandsDuped(self):
        blocks = {
                   # overlapping in the middle of a block
                   'seq1': [((0, 20, '+'), (20, 40, '-')),
                            ((10, 15, '+'), (0, 5, '+')),
                            ((20, 40, '+'), (50, 70, '-'))],
                   # overlapping and going past the end of a block
                   'seq2': [((0, 20, '+'), (20, 40, '-')),
                            ((10, 30, '+'), (0, 20, '+'))]
                 }
        merged = { 'seq1': [((0, 10, '+'), [(30, 40, '-')]),
                            ((10, 15, '+'), [(25, 30, '-'), (0, 5, '+')]),
                            ((15, 20, '+'), [(20, 25, '-')]),
                            ((20, 40, '+'), [(50, 70, '-')])],
                   'seq2': [((0, 10, '+'), [(30, 40, '-')]),
                            ((10, 20, '+'), [(20, 30, '-'), (0, 10, '+')]),
                            ((20, 30, '+'), [(10, 20, '+')])]
                 }
        self.assertEqual(self.contiguousRegions.mergeBlocks(blocks), merged)

    def test_mergeBlocks_tripled(self):
        blocks = {
            # stacking
            'seq1': [((0, 20, '+'), (20, 40, '+')),
                     ((0, 20, '+'), (70, 90, '-')),
                     ((0, 20, '+'), (50, 70, '+'))],
            # mixture of overlap types
            'seq2': [((0, 20, '+'), (20, 40, '+')),
                     ((10, 15, '+'), (60, 65, '-')),
                     ((11, 30, '+'), (80, 101, '+'))]
                 }
        merged = {
            'seq1': [((0, 20, '+'), [(20, 40, '+'), (70, 90, '-'), (50, 70, '+')])],
            'seq2': [((0, 10, '+'), [(20, 30, '+')]),
                     ((10, 11, '+'), [(30, 31, '+'), (64, 65, '-')]),
                     ((11, 15, '+'), [(31, 35, '+'), (60, 64, '-'), (80, 84, '+')]),
                     ((15, 20, '+'), [(35, 40, '+'), (84, 89, '+')]),
                     ((20, 30, '+'), [(89, 101, '+')])]
                 }
        self.assertEqual(self.contiguousRegions.mergeBlocks(blocks), merged)
if __name__ == '__main__':
    unittest.main()
