/*
 * Copyright (C) 2011--2012 Universitat d'Alacant
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#include <weight.h>
#include <lrx_compiler.h>
#include <cstdint>

using namespace std;

wstring const LRXCompiler::LRX_COMPILER_LRX_ELEM        = L"lrx";
wstring const LRXCompiler::LRX_COMPILER_DEFSEQS_ELEM    = L"def-seqs";
wstring const LRXCompiler::LRX_COMPILER_DEFSEQ_ELEM     = L"def-seq";
wstring const LRXCompiler::LRX_COMPILER_RULES_ELEM      = L"rules";
wstring const LRXCompiler::LRX_COMPILER_RULE_ELEM       = L"rule";
wstring const LRXCompiler::LRX_COMPILER_MATCH_ELEM      = L"match";
wstring const LRXCompiler::LRX_COMPILER_SELECT_ELEM     = L"select";
wstring const LRXCompiler::LRX_COMPILER_REMOVE_ELEM     = L"remove";
wstring const LRXCompiler::LRX_COMPILER_OR_ELEM         = L"or";
wstring const LRXCompiler::LRX_COMPILER_REPEAT_ELEM     = L"repeat";
wstring const LRXCompiler::LRX_COMPILER_SEQ_ELEM        = L"seq";

wstring const LRXCompiler::LRX_COMPILER_LEMMA_ATTR      = L"lemma";
wstring const LRXCompiler::LRX_COMPILER_SUFFIX_ATTR     = L"suffix";
wstring const LRXCompiler::LRX_COMPILER_CONTAINS_ATTR   = L"contains";
wstring const LRXCompiler::LRX_COMPILER_CASE_ATTR       = L"case";
wstring const LRXCompiler::LRX_COMPILER_SURFACE_ATTR    = L"surface";
wstring const LRXCompiler::LRX_COMPILER_TAGS_ATTR       = L"tags";
wstring const LRXCompiler::LRX_COMPILER_WEIGHT_ATTR     = L"weight";
wstring const LRXCompiler::LRX_COMPILER_COMMENT_ATTR    = L"c";
wstring const LRXCompiler::LRX_COMPILER_NAME_ATTR       = L"n";
wstring const LRXCompiler::LRX_COMPILER_FROM_ATTR       = L"from";
wstring const LRXCompiler::LRX_COMPILER_UPTO_ATTR       = L"upto";

wstring const LRXCompiler::LRX_COMPILER_TYPE_SELECT     = L"select";
wstring const LRXCompiler::LRX_COMPILER_TYPE_REMOVE     = L"remove";
wstring const LRXCompiler::LRX_COMPILER_TYPE_SKIP       = L"skip";

double const  LRXCompiler::LRX_COMPILER_DEFAULT_WEIGHT  = 1.0;

wstring
LRXCompiler::itow(int i)
{
  // Convert an int to a wstring
  wchar_t buf[50];
  memset(buf, '\0', sizeof(buf));
  swprintf(buf, 50, L"%d", i);
  wstring id(buf);
  return id;
}

int
LRXCompiler::wtoi(wstring w)
{
  // Convert a wstring to an int
  wistringstream wstrm(w);
  int i_name = -numeric_limits<int>::max();
  wstrm >> i_name;

  return i_name;
}

double
LRXCompiler::wtod(wstring w)
{
  // Convert a wstring to a double
  wistringstream wstrm(w);
  double d_name = -numeric_limits<double>::max();
  wstrm >> d_name;

  return d_name;
}

LRXCompiler::LRXCompiler()
{
  LtLocale::tryToSetLocale();

  debugMode = false;
  outputGraph = false;

  currentRuleId = 0;

  initialState = transducer.getInitial();
  currentState = initialState;
  lastState = initialState;

  canSelect = true;

  alphabet.includeSymbol(L"<"+ LRX_COMPILER_TYPE_SELECT + L">");
  alphabet.includeSymbol(L"<"+ LRX_COMPILER_TYPE_REMOVE + L">");
  alphabet.includeSymbol(L"<"+ LRX_COMPILER_TYPE_SKIP + L">");

  alphabet.includeSymbol(L"<ANY_TAG>");
  alphabet.includeSymbol(L"<ANY_CHAR>");
  alphabet.includeSymbol(L"<ANY_UPPER>");
  alphabet.includeSymbol(L"<ANY_LOWER>");
  alphabet.includeSymbol(L"<$>");

}

LRXCompiler::~LRXCompiler()
{
}

void
LRXCompiler::setDebugMode(bool o)
{
  debugMode = o;
}


void
LRXCompiler::setOutputGraph(bool o)
{
  outputGraph = o;
}

void
LRXCompiler::skipBlanks(wstring &name)
{
  while(name == L"#text" || name == L"#comment")
  {
    if(name != L"#comment")
    {
      if(!allBlanks())
      {
        wcerr << L"Error (" << xmlTextReaderGetParserLineNumber(reader);
        wcerr << L"): Invalid construction." << endl;
        exit(EXIT_FAILURE);
      }
    }

    xmlTextReaderRead(reader);
    name = XMLParseUtil::towstring(xmlTextReaderConstName(reader));
  }
}

wstring
LRXCompiler::attrib(wstring const &name)
{
  return XMLParseUtil::attrib(reader, name);
}

wstring
LRXCompiler::attrib(wstring const &name, const wstring fallback)
{
  string mystr = "";
  for (int i = 0, limit = name.size(); i != limit; i++) {
    mystr += static_cast<char>(name[i]);
  }

  xmlChar *attrname = xmlCharStrdup(mystr.c_str());
  xmlChar *myattr = xmlTextReaderGetAttribute(reader, attrname);
  wstring result = XMLParseUtil::towstring(myattr);
  xmlFree(myattr);
  xmlFree(attrname);
  if(myattr == NULL) {
    return fallback;
  }
  else {
    return result;
  }
}

bool
LRXCompiler::allBlanks()
{
  bool flag = true;
  wstring text = XMLParseUtil::towstring(xmlTextReaderConstValue(reader));

  for(unsigned int i = 0, limit = text.size(); i < limit; i++)
  {
    flag = flag && iswspace(text[i]);
  }

  return flag;
}

void
LRXCompiler::parse(string const &fitxer)
{
  reader = xmlReaderForFile(fitxer.c_str(), NULL, 0);
  if(reader == NULL)
  {
    cerr << "Error: Cannot open '" << fitxer << "'." << endl;
    exit(EXIT_FAILURE);
  }

  int ret = xmlTextReaderRead(reader);
  while(ret == 1)
  {
    procNode();
    ret = xmlTextReaderRead(reader);
  }

  transducer.minimize();

  if(ret != 0)
  {
    wcerr << L"Error: Parse error at the end of input." << endl;
  }

}

void
LRXCompiler::procNode()
{
  xmlChar const *xnombre = xmlTextReaderConstName(reader);
  wstring nombre = XMLParseUtil::towstring(xnombre);

  if(nombre == L"#text")
  {
    /* ignorar */
  }
  else if(nombre== L"#comment")
  {
    /* ignorar */
  }
  else if(nombre == LRX_COMPILER_LRX_ELEM)
  {
    /* ignorar */
  }
  else if(nombre == LRX_COMPILER_DEFSEQS_ELEM)
  {
    /* ignorar */
  }
  else if(nombre == LRX_COMPILER_DEFSEQ_ELEM)
  {
    procDefSeq();
  }
  else if(nombre == LRX_COMPILER_RULES_ELEM)
  {
    /* ignorar */
  }
  else if(nombre == LRX_COMPILER_RULE_ELEM)
  {
    procRule();
  }
  else
  {
    wcerr << L"Error (" << xmlTextReaderGetParserLineNumber(reader);
    wcerr << L"): Invalid node '<" << nombre << L">'." << endl;
    exit(EXIT_FAILURE);
  }

  return;
}

void
LRXCompiler::procRule()
{
  wstring comment = this->attrib(LRX_COMPILER_COMMENT_ATTR);
  wstring xweight = this->attrib(LRX_COMPILER_WEIGHT_ATTR);
  wstring nombre = this->attrib(LRX_COMPILER_NAME_ATTR);
  double weight =  wtod (xweight);

  if(weight <= -numeric_limits<int>::max())
  {
    weight = LRX_COMPILER_DEFAULT_WEIGHT ;
  }

  // insert new epsilon:epsilon step at beginning to rule to ensure that it doesn't overlap with any other rules
  currentState = transducer.insertNewSingleTransduction(alphabet(0, 0), currentState);

  currentRuleId++;
  wstring ruleId = L"<" + itow(currentRuleId) + L">";
  weights[currentRuleId] = weight;

  if(debugMode)
  {
    fwprintf(stderr, L"  rule: %d, weight: %.2f \n", currentRuleId, weight);
  }

  while(true)
  {
    int ret = xmlTextReaderRead(reader);
    if(ret != 1)
    {
      wcerr << L"Error (" << xmlTextReaderGetParserLineNumber(reader);
      wcerr << L"): Parse error." << endl;
      exit(EXIT_FAILURE);
    }

    wstring name = XMLParseUtil::towstring(xmlTextReaderConstName(reader));
    skipBlanks(name);

    if(name == LRX_COMPILER_MATCH_ELEM)
    {
      procMatch();
    }
    else if(name == LRX_COMPILER_OR_ELEM)
    {
      lastState = currentState;
      procOr();
    }
    else if(name == LRX_COMPILER_REPEAT_ELEM)
    {
      procRepeat();
    }
    else if(name == LRX_COMPILER_SEQ_ELEM)
    {
      procSeq();
    }
    else if(name == LRX_COMPILER_RULE_ELEM)
    {
      currentState = transducer.insertSingleTransduction(alphabet(alphabet(L"<$>"), alphabet(L"<$>")), currentState);
      if(!alphabet.isSymbolDefined(ruleId.c_str()))
      {
        alphabet.includeSymbol(ruleId.c_str());
      }
      currentState = transducer.insertSingleTransduction(alphabet(0, alphabet(ruleId.c_str())), currentState);
      transducer.setFinal(currentState);
      currentState = initialState;
      return;
    }
    else
    {
      wcerr << L"Error (" << xmlTextReaderGetParserLineNumber(reader);
      wcerr << L"): Invalid inclusion of '<" << name << L">' into '<" << LRX_COMPILER_RULE_ELEM;
      wcerr << L">'." << endl;
      exit(EXIT_FAILURE);
    }
  }


  return;
}

void
LRXCompiler::procOr()
{

  if(debugMode)
  {
    fwprintf(stderr, L"    or: \n");
  }

  int or_initial_state = currentState;
  vector<int> reachedStates;
  while(true)
  {
    int ret = xmlTextReaderRead(reader);
    if(ret != 1)
    {
      wcerr << L"Error (" << xmlTextReaderGetParserLineNumber(reader);
      wcerr << L"): Parse error." << endl;
      exit(EXIT_FAILURE);
    }

    wstring name = XMLParseUtil::towstring(xmlTextReaderConstName(reader));
    skipBlanks(name);

    if(name == LRX_COMPILER_MATCH_ELEM)
    {
      currentState = transducer.insertNewSingleTransduction(0, or_initial_state);
      procMatch();
      reachedStates.push_back(currentState);
    }
    else if(name == LRX_COMPILER_SEQ_ELEM)
    {
      currentState = transducer.insertNewSingleTransduction(0, or_initial_state);
      procSeq();
      reachedStates.push_back(currentState);
    }
    else if(name == LRX_COMPILER_OR_ELEM)
    {
      if(reachedStates.size() > 1)
      {
        for(auto& it : reachedStates)
        {
          if(it == currentState)
          {
            continue;
          }
          transducer.linkStates(it, currentState, 0);
        }
      }
      break;
    }
    else
    {
      wcerr << L"Error (" << xmlTextReaderGetParserLineNumber(reader);
      wcerr << L"): Invalid inclusion of '<" << name << L">' into '<" << LRX_COMPILER_OR_ELEM;
      wcerr << L">'." << endl;
      exit(EXIT_FAILURE);
    }
  }

  return;
}


void
LRXCompiler::procDefSeq()
{
  canSelect = false;
  Transducer temp = transducer;
  transducer.clear();
  int oldstate = currentState;
  currentState = initialState;
  lastState = initialState;
  wstring seqname = this->attrib(LRX_COMPILER_NAME_ATTR);
  while(true)
  {
    int ret = xmlTextReaderRead(reader);
    if(ret != 1)
    {
      wcerr << L"Error (" << xmlTextReaderGetParserLineNumber(reader);
      wcerr << L"): Parse error." << endl;
      exit(EXIT_FAILURE);
    }

    wstring name = XMLParseUtil::towstring(xmlTextReaderConstName(reader));
    skipBlanks(name);

    if(name == LRX_COMPILER_MATCH_ELEM)
    {
      procMatch();
    }
    else if(name == LRX_COMPILER_OR_ELEM)
    {
      lastState = currentState;
      procOr();
    }
    else if(name == LRX_COMPILER_REPEAT_ELEM)
    {
      procRepeat();
    }
    else if(name == LRX_COMPILER_SEQ_ELEM)
    {
      procSeq();
    }
    else if(name == LRX_COMPILER_DEFSEQ_ELEM)
    {
      transducer.setFinal(currentState);
      break;
    }
    else
    {
      wcerr << L"Error (" << xmlTextReaderGetParserLineNumber(reader);
      wcerr << L"): Invalid inclusion of '<" << name << L">' into '<" << LRX_COMPILER_REPEAT_ELEM;
      wcerr << L">'." << endl;
      exit(EXIT_FAILURE);
    }
  }
  sequences[seqname] = transducer;
  currentState = oldstate;
  lastState = oldstate;
  transducer = temp;
  canSelect = true;
}


void
LRXCompiler::procMatch()
{
  // These are mutually exclusive
  wstring lemma = this->attrib(LRX_COMPILER_LEMMA_ATTR, L"*");
  wstring contains = this->attrib(LRX_COMPILER_SUFFIX_ATTR);
  wstring suffix = this->attrib(LRX_COMPILER_CONTAINS_ATTR);
  wstring _case = this->attrib(LRX_COMPILER_CASE_ATTR); // This could potentially be non-exclusive

  // This is currently disabled: Future use
  wstring surface = this->attrib(LRX_COMPILER_SURFACE_ATTR);

  wstring tags = this->attrib(LRX_COMPILER_TAGS_ATTR, L"*");

  if(surface != L"")
  {
    if(debugMode)
    {
      fwprintf(stderr, L"      match: %S\n", surface.c_str());
    }

    for(auto& it : surface)
    {
      currentState = transducer.insertSingleTransduction(alphabet(it, 0), currentState);
    }
  }
  else
  {
    if(debugMode)
    {
      fwprintf(stderr, L"      match: [%S, %S, %S, %S] %S\n", lemma.c_str(), suffix.c_str(), contains.c_str(), _case.c_str(), tags.c_str());
    }

    if(_case != L"")
    {
      if(_case == L"AA") // <ANY_UPPER>+
      {
        int localLast = currentState;
        currentState = transducer.insertSingleTransduction(alphabet(alphabet(L"<ANY_UPPER>"), 0), currentState);
        transducer.linkStates(currentState, localLast, 0);
      }
      else if(_case == L"aa")  // <ANY_LOWER>+
      {
        int localLast = currentState;
        currentState = transducer.insertSingleTransduction(alphabet(alphabet(L"<ANY_LOWER>"), 0), currentState);
        transducer.linkStates(currentState, localLast, 0);
      }
      else if(_case == L"Aa") // <ANY_UPPER>+ <ANY_LOWER>+
      {
        currentState = transducer.insertSingleTransduction(alphabet(alphabet(L"<ANY_UPPER>"), 0), currentState);
        int localLast = currentState;
        currentState = transducer.insertSingleTransduction(alphabet(alphabet(L"<ANY_LOWER>"), 0), currentState);
        transducer.linkStates(currentState, localLast, 0);
      }
    }
    if(lemma == L"*" && suffix == L"" && contains == L"" && _case == L"")
    {
      // This is only if there is no suffix or case or contains
      if(debugMode)
      {
        fwprintf(stderr, L"        char: -\n");
      }
      int localLast = currentState;
      currentState = transducer.insertSingleTransduction(alphabet(alphabet(L"<ANY_CHAR>"), 0), currentState);
      transducer.linkStates(currentState, localLast, 0);
    }
    else if(suffix != L"")
    {
      // A suffix is <ANY_CHAR> any amount of times followed by whatever is in the suffix
      int localLast = currentState;
      currentState = transducer.insertSingleTransduction(alphabet(alphabet(L"<ANY_CHAR>"), 0), currentState);
      transducer.linkStates(currentState, localLast, 0);
      for(auto& it : suffix)
      {
        currentState = transducer.insertSingleTransduction(alphabet(it, 0), currentState);
      }
    }
    else if(contains != L"")
    {
      // A contains is <ANY_CHAR> any amount of times followed by whatever is in the attribute
      // followed by <ANY_CHAR> any amount of times
      int localLast = currentState;
      currentState = transducer.insertSingleTransduction(alphabet(alphabet(L"<ANY_CHAR>"), 0), currentState);
      transducer.linkStates(currentState, localLast, 0);
      for(auto& it : suffix)
      {
        currentState = transducer.insertSingleTransduction(alphabet(it, 0), currentState);
      }
      currentState = transducer.insertSingleTransduction(alphabet(alphabet(L"<ANY_CHAR>"), 0), currentState);
      transducer.linkStates(currentState, localLast, 0);
    }
    else if(lemma != L"*")
    {
      for(auto& it : lemma)
      {
        currentState = transducer.insertSingleTransduction(alphabet(it, 0), currentState);
      }
    }
    else
    {
      fwprintf(stderr, L"Something surprising happened in <match> compilation\n");
    }

    wstring tag = L"";
    for(auto& it : tags)
    {
      if(it == L'.')
      {
        if(tag == L"")
        {
          continue;
        }
        tag = L"<" + tag + L">";
        if(!alphabet.isSymbolDefined(tag.c_str()))
        {
          alphabet.includeSymbol(tag.c_str());
        }
        if(debugMode)
        {
          fwprintf(stderr, L"        tag: %S\n", tag.c_str());
        }
        if(tag == L"<*>")
        {
          int localLast = currentState;
          currentState = transducer.insertSingleTransduction(alphabet(alphabet(L"<ANY_TAG>"), 0), currentState);
          transducer.linkStates(currentState, localLast, 0);
        }
        else
        {
          currentState = transducer.insertSingleTransduction(alphabet(alphabet(tag.c_str()), 0), currentState);
        }
        tag = L"";
        continue;
      }
      tag = tag + it;
    }
    if(tag == L"*")
    {
      if(debugMode)
      {
        fwprintf(stderr, L"        tag: %S\n", tag.c_str());
      }
      int localLast = currentState;
      currentState = transducer.insertSingleTransduction(alphabet(alphabet(L"<ANY_TAG>"), 0), currentState);
      transducer.linkStates(currentState, localLast, 0);
    }
    else if(tag == L"")
    {
    }
    else
    {
      tag = L"<" + tag + L">";
      if(!alphabet.isSymbolDefined(tag.c_str()))
      {
        alphabet.includeSymbol(tag.c_str());
      }
      if(debugMode)
      {
        fwprintf(stderr, L"        tag: %S\n", tag.c_str());
      }
      currentState = transducer.insertSingleTransduction(alphabet(alphabet(tag.c_str()), 0), currentState);
    }
  }

  if(xmlTextReaderIsEmptyElement(reader))
  {
    // If self-closing
    currentState = transducer.insertSingleTransduction(alphabet(alphabet(L"<$>"), alphabet(L"<$>")), currentState);
    currentState = transducer.insertSingleTransduction(alphabet(0, alphabet(L"<skip>")), currentState);
    return;
  }

  wstring name = L"";
  while(true)
  {
    int ret = xmlTextReaderRead(reader);
    if(ret != 1)
    {
      wcerr << L"Error (" << xmlTextReaderGetParserLineNumber(reader);
      wcerr << L"): Parse error." << endl;
      exit(EXIT_FAILURE);
    }

    name = XMLParseUtil::towstring(xmlTextReaderConstName(reader));
    skipBlanks(name);

    if(name == LRX_COMPILER_SELECT_ELEM)
    {
      if(!canSelect)
      {
        wcerr << L"Error (" << xmlTextReaderGetParserLineNumber(reader);
        wcerr << L"): <select> is not permitted inside <repeat>." << endl;
        exit(EXIT_FAILURE);
      }
      procSelect();
    }
    else if(name == LRX_COMPILER_REMOVE_ELEM)
    {
      if(!canSelect)
      {
        wcerr << L"Error (" << xmlTextReaderGetParserLineNumber(reader);
        wcerr << L"): <remove> is not permitted inside <repeat>." << endl;
        exit(EXIT_FAILURE);
      }
      procRemove();
    }
    else if(name == LRX_COMPILER_MATCH_ELEM)
    {
      return;
    }
    else
    {
      wcerr << L"Error (" << xmlTextReaderGetParserLineNumber(reader);
      wcerr << L"): Invalid inclusion of '<" << name << L">' into '<" << LRX_COMPILER_MATCH_ELEM;
      wcerr << L">'." << endl;
      exit(EXIT_FAILURE);
    }
  }


  return;
}

void
LRXCompiler::procSelect()
{

  wstring lemma =this->attrib(LRX_COMPILER_LEMMA_ATTR, L"*");
  wstring tags =this->attrib(LRX_COMPILER_TAGS_ATTR);

  wstring key = L"<" + LRX_COMPILER_TYPE_SELECT + L">";
  if(lemma != L"*")
  {
    key += lemma;
  }

  Transducer recogniser;
  int localCurrentState = recogniser.getInitial();

  if(debugMode)
  {
    fwprintf(stderr, L"        select: %S, %S\n", lemma.c_str(), tags.c_str());
  }

  currentState = transducer.insertSingleTransduction(alphabet(alphabet(L"<$>"), alphabet(L"<$>")), currentState);
  currentState = transducer.insertSingleTransduction(alphabet(0, alphabet(L"<" + LRX_COMPILER_TYPE_SELECT + L">")), currentState);


  if(lemma == L"*")
  {
    currentState = transducer.insertSingleTransduction(alphabet(0, alphabet(L"<ANY_CHAR>")), currentState);
    int localLast = localCurrentState;
    localCurrentState = recogniser.insertSingleTransduction(alphabet(alphabet(L"<ANY_CHAR>"),0), localCurrentState);
    recogniser.linkStates(localCurrentState, localLast, 0);
    key = key + L"<ANY_CHAR>";
  }
  else {
    for (auto &it : lemma) {
      currentState = transducer.insertSingleTransduction(alphabet(0, it), currentState);
      localCurrentState = recogniser.insertSingleTransduction(alphabet(it, 0), localCurrentState);
    }
  }

  if(tags != L"")
  {
    wstring tag = L"";
    for(auto& it : tags)
    {
      if(it == L'.')
      {
        tag = L"<" + tag + L">";
        if(!alphabet.isSymbolDefined(tag.c_str()))
        {
          alphabet.includeSymbol(tag.c_str());
        }
        if(debugMode)
        {
          fwprintf(stderr, L"        tag: %S\n", tag.c_str());
        }
        if(tag == L"<*>")
        {
          currentState = transducer.insertSingleTransduction(alphabet(0, alphabet(L"<ANY_TAG>")), currentState);
	  int localLast = localCurrentState;
          localCurrentState = recogniser.insertSingleTransduction(alphabet(alphabet(L"<ANY_TAG>"),0), localCurrentState);
	  recogniser.linkStates(localCurrentState, localLast, 0);
          key = key + L"<ANY_TAG>";
        }
        else
        {
          currentState = transducer.insertSingleTransduction(alphabet(0, alphabet(tag.c_str())), currentState);
          localCurrentState = recogniser.insertSingleTransduction(alphabet(alphabet(tag.c_str()),0), localCurrentState);
          key = key + tag;
        }
        tag = L"";
        continue;
      }
      tag = tag + it;
    }
    if(tag == L"*")
    {
      if(debugMode)
      {
        fwprintf(stderr, L"        tag: %S\n", tag.c_str());
      }
      currentState = transducer.insertSingleTransduction(alphabet(0, alphabet(L"<ANY_TAG>")), currentState);
      int localLast = localCurrentState;
      localCurrentState = recogniser.insertSingleTransduction(alphabet(alphabet(L"<ANY_TAG>"),0), localCurrentState);
      recogniser.linkStates(localCurrentState, localLast, 0);
      key = key + L"<ANY_TAG>";
    }
    else
    {
      tag = L"<" + tag + L">";
      if(!alphabet.isSymbolDefined(tag.c_str()))
      {
        alphabet.includeSymbol(tag.c_str());
      }
      if(debugMode)
      {
        fwprintf(stderr, L"        tag: %S\n", tag.c_str());
      }
      currentState = transducer.insertSingleTransduction(alphabet(0, alphabet(tag.c_str())), currentState);
      localCurrentState = recogniser.insertSingleTransduction(alphabet(alphabet(tag.c_str()),0), localCurrentState);
      key = key + tag;
    }
  }
  else
  {
    if(debugMode)
    {
      fwprintf(stderr, L"        tag: -\n");
    }
    currentState = transducer.insertSingleTransduction(alphabet(0, alphabet(L"<ANY_TAG>")), currentState);
    int localLast = localCurrentState;
    localCurrentState = recogniser.insertSingleTransduction(alphabet(alphabet(L"<ANY_TAG>"),0), localCurrentState);
    recogniser.linkStates(localCurrentState, localLast, 0);
    key = key + L"<ANY_TAG>";
  }


  recogniser.setFinal(localCurrentState);

  recognisers[key] = recogniser;
  if(debugMode)
  {
    fwprintf(stderr, L"        select: %d\n", recognisers[key].size());
  }
  //currentState = transducer.insertSingleTransduction(alphabet(alphabet(L"<$>"), alphabet(L"<$>")), currentState);

  return;
}

void
LRXCompiler::procRemove()
{

  wstring lemma =this->attrib(LRX_COMPILER_LEMMA_ATTR, L"*");
  wstring tags =this->attrib(LRX_COMPILER_TAGS_ATTR);

  wstring key = L"<" + LRX_COMPILER_TYPE_REMOVE + L">";
  if(lemma != L"*")
  {
    key += lemma;
  }

  Transducer recogniser;
  int localCurrentState = recogniser.getInitial();

  if(debugMode)
  {
    fwprintf(stderr, L"        remove: %S, %S\n", lemma.c_str(), tags.c_str());
  }

  currentState = transducer.insertSingleTransduction(alphabet(alphabet(L"<$>"), alphabet(L"<$>")), currentState);
  currentState = transducer.insertSingleTransduction(alphabet(0, alphabet(L"<" + LRX_COMPILER_TYPE_REMOVE + L">")), currentState);

  if(lemma == L"*")
  {
    currentState = transducer.insertSingleTransduction(alphabet(0, alphabet(L"<ANY_CHAR>")), currentState);
    int localLast = localCurrentState;
    localCurrentState = recogniser.insertSingleTransduction(alphabet(alphabet(L"<ANY_CHAR>"),0), localCurrentState);
    recogniser.linkStates(localCurrentState, localLast, 0);
    key = key + L"<ANY_CHAR>";
  }
  else
  {
    for(auto& it : lemma)
    {
      currentState = transducer.insertSingleTransduction(alphabet(0, it), currentState);
      localCurrentState = recogniser.insertSingleTransduction(alphabet(it, 0), localCurrentState);
    }
  }

  if(tags != L"")
  {
    wstring tag = L"";
    for(auto& it : tags)
    {
      if(it == L'.')
      {
        tag = L"<" + tag + L">";
        if(!alphabet.isSymbolDefined(tag.c_str()))
        {
          alphabet.includeSymbol(tag.c_str());
        }
        if(debugMode)
        {
          fwprintf(stderr, L"        tag: %S\n", tag.c_str());
        }
        if(tag == L"<*>")
        {
          currentState = transducer.insertSingleTransduction(alphabet(0, alphabet(L"<ANY_TAG>")), currentState);
	  int localLast = localCurrentState;
          localCurrentState = recogniser.insertSingleTransduction(alphabet(alphabet(L"<ANY_TAG>"),0), localCurrentState);
	  recogniser.linkStates(localCurrentState, localLast, 0);
          key = key + L"<ANY_TAG>";
        }
        else
        {
          currentState = transducer.insertSingleTransduction(alphabet(0, alphabet(tag.c_str())), currentState);
          localCurrentState = recogniser.insertSingleTransduction(alphabet(alphabet(tag.c_str()),0), localCurrentState);
          key = key + tag;
        }
        tag = L"";
        continue;
      }
      tag = tag + it;
    }
    if(tag == L"*")
    {
      if(debugMode)
      {
        fwprintf(stderr, L"        tag: %S\n", tag.c_str());
      }
      currentState = transducer.insertSingleTransduction(alphabet(0, alphabet(L"<ANY_TAG>")), currentState);
      int localLast = localCurrentState;
      localCurrentState = recogniser.insertSingleTransduction(alphabet(alphabet(L"<ANY_TAG>"),0), localCurrentState);
      recogniser.linkStates(localCurrentState, localLast, 0);
      key = key + L"<ANY_TAG>";
    }
    else
    {
      tag = L"<" + tag + L">";
      if(!alphabet.isSymbolDefined(tag.c_str()))
      {
        alphabet.includeSymbol(tag.c_str());
      }
      if(debugMode)
      {
        fwprintf(stderr, L"        tag: %S\n", tag.c_str());
      }
      currentState = transducer.insertSingleTransduction(alphabet(0, alphabet(tag.c_str())), currentState);
      localCurrentState = recogniser.insertSingleTransduction(alphabet(alphabet(tag.c_str()),0), localCurrentState);
      key = key + tag;
    }
  }
  else
  {
    if(debugMode)
    {
      fwprintf(stderr, L"        tag: -\n");
    }
    currentState = transducer.insertSingleTransduction(alphabet(0, alphabet(L"<ANY_TAG>")), currentState);
    int localLast = localCurrentState;
    localCurrentState = recogniser.insertSingleTransduction(alphabet(alphabet(L"<ANY_TAG>"),0), localCurrentState);
    recogniser.linkStates(localCurrentState, localLast, 0);
    key = key + L"<ANY_TAG>";
  }


  recogniser.setFinal(localCurrentState);

  recognisers[key] = recogniser;
  if(debugMode)
  {
    fwprintf(stderr, L"        remove: %d\n", recognisers[key].size());
  }

  return;
}


void
LRXCompiler::procRepeat()
{
  bool couldSelect = canSelect;
  canSelect = false;
  wstring xfrom = this->attrib(LRX_COMPILER_FROM_ATTR);
  wstring xupto = this->attrib(LRX_COMPILER_UPTO_ATTR);
  int from = stoi(xfrom);
  int upto = stoi(xupto);
  if(from < 0 || upto < 0)
  {
    wcerr << L"Error (" << xmlTextReaderGetParserLineNumber(reader);
    wcerr << L"): Number of repetitions cannot be negative." << endl;
    exit(EXIT_FAILURE);
  }
  else if(from > upto)
  {
    wcerr << L"Error (" << xmlTextReaderGetParserLineNumber(reader);
    wcerr << L"): Lower bound on number of repetitions cannot be larger than upper bound." << endl;
    exit(EXIT_FAILURE);
  }
  int count = upto - from;
  int oldstate = currentState;
  Transducer temp = transducer;
  transducer.clear();
  currentState = initialState;
  lastState = initialState;
  while(true)
  {
    int ret = xmlTextReaderRead(reader);
    if(ret != 1)
    {
      wcerr << L"Error (" << xmlTextReaderGetParserLineNumber(reader);
      wcerr << L"): Parse error." << endl;
      exit(EXIT_FAILURE);
    }

    wstring name = XMLParseUtil::towstring(xmlTextReaderConstName(reader));
    skipBlanks(name);

    if(name == LRX_COMPILER_MATCH_ELEM)
    {
      procMatch();
    }
    else if(name == LRX_COMPILER_OR_ELEM)
    {
      lastState = currentState;
      procOr();
    }
    else if(name == LRX_COMPILER_SEQ_ELEM)
    {
      procSeq();
    }
    else if(name == LRX_COMPILER_REPEAT_ELEM)
    {
      transducer.setFinal(currentState);
      break;
    }
    else
    {
      wcerr << L"Error (" << xmlTextReaderGetParserLineNumber(reader);
      wcerr << L"): Invalid inclusion of '<" << name << L">' into '<" << LRX_COMPILER_REPEAT_ELEM;
      wcerr << L">'." << endl;
      exit(EXIT_FAILURE);
    }
  }
  for(int i = 0; i < from; i++)
  {
    oldstate = temp.insertTransducer(oldstate, transducer);
  }
  transducer.optional();
  for(int i = 0; i < count; i++)
  {
    oldstate = temp.insertTransducer(oldstate, transducer);
  }
  currentState = oldstate;
  lastState = oldstate;
  transducer = temp;
  canSelect = couldSelect;
}


void
LRXCompiler::procSeq()
{
  wstring name = this->attrib(LRX_COMPILER_NAME_ATTR);
  if(sequences.find(name) == sequences.end())
  {
    wcerr << L"Error (" << xmlTextReaderGetParserLineNumber(reader);
    wcerr << L"): Sequence '" << name << L"' not defined." << endl;
    exit(EXIT_FAILURE);
  }
  currentState = transducer.insertTransducer(currentState, sequences[name]);
}


void
LRXCompiler::write(FILE *fst)
{
  alphabet.write(fst);

  Compression::multibyte_write(recognisers.size(), fst);
  for(auto& it : recognisers)
  {
    Compression::wstring_write(it.first, fst);
    if(debugMode)
    {
      fwprintf(stderr, L"+ %d => %S\n", it.second.size(), it.first.c_str());
      it.second.show(alphabet, stderr, 0, false);
    }
    it.second.write(fst);
  }

  Compression::wstring_write(L"main", fst);
  if(outputGraph)
  {
    transducer.show(alphabet, stderr, 0, false);
  }
  transducer.write(fst);

  for(auto& it : weights)
  {
    if(debugMode)
    {
      fwprintf(stderr, L"%.4f %d\n", it.second, it.first);
    }
    weight record{it.first, "", it.second};
    weight_to_le(record);
    fwrite((void *)&record, 1, sizeof(weight), fst);
  }

  if(!outputGraph)
  {
    fwprintf(stderr, L"%d: %d@%d\n", currentRuleId, transducer.size(), transducer.numberOfTransitions());
  }
}