/*
 * Copyright (C) 2013 by Glenn Hickey (hickey@soe.ucsc.edu)
 *
 * Released under the MIT license, see LICENSE.txt
 */

#include <deque>
#include <cassert>
#include "halWiggleLiftover.h"
#include "halBlockMapper.h"

using namespace std;
using namespace hal;

const double WiggleLiftover::DefaultValue = 0.0;
const hal_size_t WiggleLiftover::DefaultTileSize = 10000;

WiggleLiftover::WiggleLiftover()
{

}

WiggleLiftover::~WiggleLiftover()
{

}

void WiggleLiftover::convert(AlignmentConstPtr alignment,
                             const Genome* srcGenome,
                             istream* inputFile,
                             const Genome* tgtGenome,
                             ostream* outputFile,
                             bool traverseDupes,
                             bool unique)
{
  _alignment = alignment;
  _srcGenome = srcGenome;
  _tgtGenome = tgtGenome;
  _outStream = outputFile;
  _traverseDupes = traverseDupes;
  _unique = unique;
  _srcSequence = NULL;

  if (_srcGenome->getNumTopSegments() > 0)
  {
    _segment = _srcGenome->getTopSegmentIterator();
    _lastIndex = (hal_index_t)_srcGenome->getNumTopSegments();
  }
  else
  {
    _segment = _srcGenome->getBottomSegmentIterator();
    _lastIndex = (hal_index_t)_srcGenome->getNumBottomSegments();
  }

  set<const Genome*> inputSet;
  inputSet.insert(_srcGenome);
  inputSet.insert(_tgtGenome);
  getGenomesInSpanningTree(inputSet, _tgtSet);
  _outVals.init(srcGenome->getSequenceLength(), DefaultValue, DefaultTileSize);
  scan(inputFile);
  write();
  _outVals.clear();
}

void WiggleLiftover::visitHeader()
{
  mapSegment();
  _srcSequence = _srcGenome->getSequence(_sequenceName);
  if (_srcSequence == NULL)
  {
    stringstream ss;
    ss << "Sequence " << _sequenceName << " not found in genome " 
       << _srcGenome->getName();
    throw hal_exception(ss.str());
  }
}

void WiggleLiftover::visitLine()
{
  if (_srcSequence == NULL)
  {
    throw hal_exception("Missing Wig header");
  }
  hal_index_t absFirst = _first + _srcSequence->getStartPosition();
  hal_index_t absLast = _last + _srcSequence->getStartPosition();
  if (absFirst < _segment->getStartPosition() || 
      absLast > _segment->getStartPosition())
  {
    mapSegment();
  }
  CoordVal cv = {absFirst, absLast, _value};
  if (_cvals.size() > 0 && _cvals.back()._last >= absFirst)
  {
    throw hal_exception("Coordinate out of order");
  }
  _cvals.push_back(cv);
  cout << "pushing cv " << cv._first << "," <<cv._last << "," << cv._val 
       << endl;
}
               
void WiggleLiftover::visitEOF()
{
  mapSegment();
}       

void WiggleLiftover::mapSegment()
{
  if (_cvals.empty())
  {
    return;
  }
  if (_segment->getArrayIndex() == 0)
  {
    _segment->toSite(_cvals[0]._first, false);
  }
  while (_segment->getStartPosition() < _cvals[0]._first)
  {
    _segment->toRight();
  }

  while (_cvals[0]._first < _segment->getStartPosition())
  {
    assert(_segment->getArrayIndex() > 0);
    assert(_segment->getReversed() == false);
    _segment->toLeft(_cvals[0]._first);
  }
  assert(_cvals[0]._first <= _segment->getEndPosition());
  if (_cvals[0]._first > _segment->getStartPosition())
  {
    hal_offset_t so = _cvals[0]._first - _segment->getStartPosition();
    _segment->slice(so, _segment->getEndOffset());
  }
  if (_cvals.back()._last > _segment->getEndPosition())
  {
    hal_offset_t eo = _segment->getEndPosition() - _cvals.back()._last;
    _segment->slice(_segment->getStartOffset(), eo);
  }

  while (_segment->getArrayIndex() < _lastIndex &&
         _segment->getStartPosition() <= (_cvals.back()._last))
  {
    _segment->getMappedSegments(_mappedSegments, _tgtGenome, &_tgtSet,
                                _traverseDupes);  
    _segment->toRight(_cvals.back()._last);
  }

  vector<MappedSegmentConstPtr> fragments;
  BlockMapper::MSSet emptySet;
  set<hal_index_t> queryCutSet;
  set<hal_index_t> targetCutSet;
  
  _cvIdx = 0;
  for (std::set<MappedSegmentConstPtr>::iterator i = _mappedSegments.begin();
       i != _mappedSegments.end(); ++i)
  {
    BlockMapper::extractSegment(i, emptySet, fragments, &_mappedSegments, 
                                targetCutSet, queryCutSet);
    mapFragments(fragments);
  }
}

void WiggleLiftover::mapFragments(vector<MappedSegmentConstPtr>& fragments)
{
  sort(fragments.begin(), fragments.end(), MappedSegment::LessSource());

  for (size_t i = 0; i < fragments.size() && _cvIdx < _cvals.size(); ++i)
  {
    MappedSegmentConstPtr& seg = fragments[i];
    for (size_t j = 0; j < seg->getLength() && _cvIdx < _cvals.size(); ++j)
    {
      hal_index_t pos = seg->getSource()->getStartPosition() + j;
      hal_index_t mpos;
      if (seg->getReversed() == false)
      {
        mpos = seg->getStartPosition() + j;
      }
      else
      {
        mpos = seg->getEndPosition() - j;
      }
      assert(_cvals[_cvIdx]._first <= pos);
      while (_cvIdx < _cvals.size() && _cvals[_cvIdx]._last < pos)
      {
        ++_cvIdx;
      }
      if (_cvIdx < _cvals.size() && _cvals[_cvIdx]._first <= pos && 
          _cvals[_cvIdx]._last >= pos)
      {
        double val = std::max(_cvals[_cvIdx]._val, _outVals.get(mpos));
        _outVals.set(mpos, val);
      }
    }
  }
}

void WiggleLiftover::write()
{
  const Sequence* outSequence = NULL;
  hal_size_t ogSize = _tgtGenome->getSequenceLength();
  for (hal_size_t i = 0; i < _outVals.getNumTiles(); ++i)
  {
    if (_outVals.isTileEmpty(i) == false)
    { 
      hal_index_t pos = i * _outVals.getTileSize();
      for (hal_size_t j = 0; pos < ogSize && j < _outVals.getTileSize(); ++j, 
              ++pos)
      {
        if (outSequence == NULL || pos < outSequence->getStartPosition() ||
            pos > outSequence->getEndPosition())
        {
          outSequence = _tgtGenome->getSequenceBySite(pos);
          assert(outSequence != NULL);
          *_outStream << "fixedStep"
                      << "\tchrom=" << outSequence->getName()
                      << "\tstart=" << (pos - outSequence->getStartPosition())
                      << "\tstep=1\n";
        }
        *_outStream << _outVals.get(pos) << '\n';
      }
    }
  }
}
