/* real.cpp: real numbers */
/*
    Copyright (C) 2008 Wolf Lammen.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License , or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; see the file COPYING.  If not, write to:

      The Free Software Foundation, Inc.
      59 Temple Place, Suite 330
      Boston, MA 02111-1307 USA.


    You may contact the author by:
       e-mail:  ookami1 <at> gmx <dot> de
       mail:  Wolf Lammen
              Oertzweg 45
              22307 Hamburg
              Germany

*************************************************************************/

#include "variant/real.hxx"
#include "math/floatconfig.h"
#include "math/floatconvert.h"
#include <QtXml/QDomText>

const char* VariantIntf::nLongReal = "LongReal";
VariantIntf::VariantType LongReal::vtLongReal;

static floatstruct NaNVal;
static int longrealPrec;
static LongReal::EvalMode longrealEvalMode;

static int _cvtMode(FmtMode mode)
{
  switch (mode)
  {
    case FixPoint: return IO_MODE_FIXPOINT;
    case Engineering: return IO_MODE_ENG;
    case Complement2: return IO_MODE_COMPLEMENT;
    default: return IO_MODE_SCIENTIFIC;
  }
}

static Sign _cvtSign(signed char sign)
{
  switch (sign)
  {
    case IO_SIGN_COMPLEMENT: return Compl2;
    case IO_SIGN_MINUS     : return Minus;
    case IO_SIGN_PLUS      : return Plus;
    default                : return None;
  }
}

static signed char _cvtSign(Sign sign)
{
  switch (sign)
  {
    case Compl2: return IO_SIGN_COMPLEMENT;
    case Minus : return IO_SIGN_MINUS;
    case Plus  : return IO_SIGN_PLUS;
    default    : return IO_SIGN_NONE;
  }
}

// static char _mod(floatnum dest, cfloatnum dividend, cfloatnum modulo)
// {
//   enum { maxdivloops = 250 };
//   int save = float_setprecision(int(maxdivloops));
//   floatstruct dummy;
//   float_create(&dummy);
//   char result = float_divmod(&dummy, dest, dividend, modulo, INTQUOT);
//   float_free(&dummy);
//   float_setprecision(save);
//   return result;
// }
// 
// static char _idiv(floatnum dest, cfloatnum dividend, cfloatnum modulo)
// {
//   int save = float_setprecision(DECPRECISION);
//   floatstruct dummy;
//   float_create(&dummy);
//   char result = float_divmod(dest, &dummy, dividend, modulo, INTQUOT);
//   float_free(&dummy);
//   float_setprecision(save);
//   return result;
// }

void LongReal::initClass()
{
  precision(PrecDefault);
  evalMode(EvalRelaxed);
  float_create(&NaNVal);
  vtLongReal = registerType(create, nLongReal);
}

VariantData* LongReal::create()
{
  return new LongReal;
}

LongReal* LongReal::create(floatnum f)
{
  LongReal* lr = static_cast<LongReal*>(create());
  lr->move(f);
  return lr;
}

cfloatnum LongReal::NaN()
{
  return &NaNVal;
}

bool LongReal::move(floatnum x)
{
  if (!isUnique())
    return false;
  float_move(&val, x);
  return true;
}

int LongReal::precision(int newprec)
{
  int result = longrealPrec;
  if (newprec == PrecDefault || newprec > DECPRECISION)
    newprec = DECPRECISION;
  if (newprec != PrecQuery)
    longrealPrec = newprec;
  return result;
}

LongReal::EvalMode LongReal::evalMode(EvalMode newMode)
{
  EvalMode result = longrealEvalMode;
  if (newMode != EvalQuery)
    longrealEvalMode = newMode;
  return result;
}

bool LongReal::isNaN() const
{
  return float_isnan(&val);
}

bool LongReal::isZero() const
{
  return float_iszero(&val);
}

int LongReal::evalPrec()
{
  return longrealPrec + 5;
}

int LongReal::workPrec()
{
  return longrealPrec + 3;
}

void LongReal::xmlWrite(QDomDocument& doc, QDomNode& parent) const
{
  // fill a buffer with a description of val in ASCII
  int lg = float_getlength(&val) + BITS_IN_EXP + 2;
  QByteArray buf(lg, 0);
  float_getscientific(buf.data(), lg, &val);

  // copy the buffer to the text portion of the given element
  parent.appendChild(doc.createTextNode(buf));
}

bool LongReal::xmlRead(QDomNode& node)
{
  // pre: node is an element
  QByteArray buf = node.toElement().text().toUtf8();
  float_setscientific(&val, buf.data(), NULLTERMINATED);
  return !float_isnan(&val);
}

RawFloatIO LongReal::convert(int digits, FmtMode mode,
                   char base, char scalebase) const
{
  t_otokens tokens;
  floatstruct workcopy;
  RawFloatIO result;
  char intpart[BINPRECISION+5];
  char fracpart[BINPRECISION+5];

  tokens.intpart.buf = intpart;
  tokens.intpart.sz = sizeof(intpart);
  tokens.fracpart.buf = fracpart;
  tokens.fracpart.sz = sizeof(fracpart);
  float_create(&workcopy);
  float_copy(&workcopy, &val, evalPrec());
  result.error = float_out(&tokens, &workcopy, digits,
                           base, scalebase, _cvtMode(mode));
  if (result.error == Success)
  {
    if (tokens.exp >= 0)
    {
      result.scale = tokens.exp;
      result.signScale = tokens.exp > 0? Plus : None;
    }
    else
    {
      result.scale = -tokens.exp;
      result.signScale = Minus;
    }
    result.baseSignificand = base;
    result.baseScale = scalebase;
    result.signSignificand = _cvtSign(tokens.sign);
    result.intpart = QString::fromAscii(tokens.intpart.buf);
    result.fracpart = QString::fromAscii(tokens.fracpart.buf);
  }
  return result;
}

Variant LongReal::convert(const RawFloatIO& io)
{
  if (io.error != Success)
    return io.error;
  t_itokens tokens;
  QByteArray intpart = io.intpart.toUtf8();
  QByteArray fracpart = io.fracpart.toUtf8();
  tokens.intpart = intpart.data();
  tokens.fracpart = intpart.data();
  tokens.sign = _cvtSign(io.signSignificand);
  tokens.base = io.baseSignificand;
  tokens.exp = 0;
  tokens.expbase = IO_BASE_NAN;
  tokens.expsign = IO_SIGN_NONE;
  if (io.scale != 0)
  {
    tokens.exp = io.scale;
    tokens.expbase = io.baseScale;
    tokens.expsign = io.signScale;
  }
  tokens.maxdigits = evalPrec();
  floatstruct val;
  float_create(&val);
  Error e = float_in(&val, &tokens);
  return Variant(&val, e);
}

static bool _isZero(const QString& str)
{
  return str.size() == 1 && str.at(0) == '0';
}

RealFormat::RealFormat()
{
  setMode(Scientific);
  setGroupChars();
  setMinLengths();
  setFlags(fShowRadix|fShowScaleRadix|fShowZeroScale);
}

void RealFormat::setMode(FmtMode m, int dgt, char b, char sb)
{
  mode = m;
  base = b;
  scalebase = sb;
  int maxdgt;
  switch (b)
  {
    case  2: maxdgt = BINPRECISION;
    case  8: maxdgt = OCTPRECISION;
    case 16: maxdgt = HEXPRECISION;
    default: maxdgt = DECPRECISION;
  }
  if (dgt <= 0 || dgt > maxdgt)
    digits = maxdgt;
  else
    digits = dgt;
}

void RealFormat::setGroupChars(QChar newdot, QChar newgroup, int newgrouplg)
{
  dot = newdot;
  groupchar = newgroup;
  grouplg = newgrouplg;
}

void RealFormat::setMinLengths(int newMinInt, int newMinFrac, int newMinScale)
{
  minIntLg = newMinInt;
  minFracLg = newMinFrac;
  minScaleLg = newMinScale;
}

void RealFormat::setFlags(unsigned flags)
{
  showZeroScale = flags & fShowZeroScale;
  showPlus = flags & fShowPlus;
  showScalePlus = flags & fShowScalePlus;
  showRadix = flags & fShowRadix;
  showScaleRadix = flags & fShowScaleRadix;
  showLeadingZero = flags & fShowLeadingZero;
  showScaleLeadingZero = flags & fShowScaleLeadingZero;
  showTrailingZero = flags & fShowTrailingZero;
  showTrailingDot = flags & fShowTrailingDot;
  lowerCaseHexDigit = flags & fLowerCaseDigit;
}

QString RealFormat::getPrefix(Sign sign, char base,
                              bool isCompl)
{
  QString result;
  const char* radix;
  switch (sign)
  {
    case None:
    case Plus:
      if (showPlus)
        result = '+';
      break;
    default:
      result = '-';
  }
  if (showRadix)
  {
    switch (base)
    {
      case 16: radix = "0x"; break;
      case  8: radix = "0o"; break;
      case  2: radix = "0b"; break;
      default: radix = "0d";
    }
    if (isCompl)
      result = radix + result;
    else
      result += radix;
  }
  if (lowerCaseHexDigit)
    result = result.toUpper();
  return result;
}

QString RealFormat::getSignificandPrefix(RawFloatIO& io)
{
  return getPrefix(io.signSignificand, io.baseSignificand,
                   mode == Complement2);
}

QString RealFormat::getSignificandSuffix(RawFloatIO& io)
{
  return QString();
}

QString RealFormat::getScalePrefix(RawFloatIO& io)
{
  QString result('e');
  if (io.baseSignificand != 10)
    result = '(';
  return result
         + getPrefix(io.signScale, io.baseScale, false);
}

QString RealFormat::getScaleSuffix(RawFloatIO& io)
{
  if (io.baseSignificand == 10)
    return QString();
  return QString(')');
}

QString RealFormat::formatNaN()
{
  return "NaN";
}

QString RealFormat::formatZero()
{
  const char* result;
  if (showTrailingDot)
    result = "0.";
  else
    result = "0";
  return result;
}

QString RealFormat::formatInt(RawFloatIO& io)
{
  if (minIntLg <= 0 &&!showLeadingZero && _isZero(io.intpart))
    return QString();
  QString intpart = io.intpart;
  if (minIntLg < intpart.size())
  {
    QChar pad = ' ';
    if (showLeadingZero)
      pad = '0';
    if (io.signSignificand == Compl2)
      switch (io.baseSignificand)
      {
        case 2 : pad = '1'; break;
        case 8 : pad = '7'; break;
        case 16: pad = 'F'; break;
      }
    intpart = QString(minIntLg - intpart.size(), pad) + intpart;
  }
  if (lowerCaseHexDigit)
    intpart = intpart.toLower();
  if (!useGrouping() || grouplg >= intpart.size())
    return intpart;
  QString result;
  int idx = intpart.size();
  while (idx > 0)
  {
    idx -= grouplg;
    QChar gchar = ' ';
    if (intpart.at(idx) != ' ')
      gchar = groupchar;
    if (idx <= 0)
      result = intpart.mid(0, idx + grouplg) + result;
    else
      result = intpart.mid(idx, grouplg) + gchar + result;
  }
  return result;
}

QString RealFormat::formatFrac(RawFloatIO& io)
{
  QString fracpart = io.fracpart;
  if (fracpart.size() < minFracLg)
  {
    QChar pad = ' ';
    if (showTrailingZero)
      pad = '0';
    fracpart += QString(minFracLg - fracpart.size());
  }
  else if (!showTrailingZero)
  {
    int i = fracpart.size();
    while (--i >= minFracLg && fracpart.at(i) == '0');
    fracpart = fracpart.mid(0, i);
  }
  if (fracpart.isEmpty())
  {
    if (showTrailingDot)
      return dot;
    else
      return QString();
  }
  if (lowerCaseHexDigit)
    fracpart = fracpart.toLower();
  if (!useGrouping() || fracpart.size() <= grouplg)
    return dot + fracpart;
  QString result = dot;
  result += fracpart.mid(0, grouplg);
  for (int idx = grouplg; idx < fracpart.size(); idx += grouplg)
  {
    QChar gchar = ' ';
    if (fracpart.at(idx) != ' ')
      gchar = groupchar;
    result += gchar + fracpart.mid(idx, grouplg);
  }
  return result;
}

QString RealFormat::formatScale(RawFloatIO& io)
{
  QString result = QString::number(io.scale, (int)io.baseScale);
  if (io.baseScale == 16 && !lowerCaseHexDigit)
    result = result.toUpper();
  if (result.size() < minScaleLg)
    result = QString(result.size() - minScaleLg,
                     showScaleLeadingZero? '0' : ' ') + result;
  return result;
}

QString RealFormat::format(const Variant& val)
{
  QString result;
  const LongReal* vr = dynamic_cast<const LongReal*>((const VariantData*)val);
  if (!vr)
    return result;
  if (vr->isNaN())
    return formatNaN();
  if (vr->isZero())
    return formatZero();
  RawFloatIO RawFloatIO = vr->convert(digits, mode, base, scalebase);
  if (RawFloatIO.error != Success)
    return result;
  result = getSignificandPrefix(RawFloatIO)
           + formatInt(RawFloatIO)
           + formatFrac(RawFloatIO)
           + getSignificandSuffix(RawFloatIO);
  if (useScale(RawFloatIO))
    result += getScalePrefix(RawFloatIO)
           + formatScale(RawFloatIO)
           + getScaleSuffix(RawFloatIO);
  return result;
}

bool RealFormat::useScale(const RawFloatIO& io)
{
  return io.scale != 0 || showZeroScale;
}
