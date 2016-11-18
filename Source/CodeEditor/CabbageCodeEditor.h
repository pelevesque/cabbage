/*
  Copyright (C) 2016 Rory Walsh

  Cabbage is free software; you can redistribute it
  and/or modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  Cabbage is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with Csound; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
  02111-1307 USA
*/
#ifndef CABBAGECODEEDITOR_H_INCLUDED
#define CABBAGECODEEDITOR_H_INCLUDED

#include "../CabbageIds.h"
#include "CsoundTokeniser.h"


class CabbageCodeEditorComponent : 
public CodeEditorComponent, 
public CodeDocument::Listener,
public ListBoxModel
{
public:
    CabbageCodeEditorComponent(Component* statusBar, ValueTree valueTree, CodeDocument &document, CodeTokeniser *codeTokeniser);
    ~CabbageCodeEditorComponent(){};
	void updateColourScheme();
    CodeDocument::Position positionInCode;
	ValueTree valueTree;
	void codeDocumentTextDeleted(int,int){};
    void codeDocumentTextInserted(const juce::String &,int);
	void displayOpcodeHelpInStatusBar(String lineFromCsd);
	String getLineText();
	bool keyPressed (const KeyPress& key) override;
	void undoText();
	
	String getSelectedText();
	const StringArray getAllTextAsStringArray();	
	StringArray getSelectedTextArray();
	

	void handleTabKey(String direction);
	void handleReturnKey();
	
	void insertCode(int lineNumber, String codeToInsert, bool replaceExistingLine, bool highlightLine);
	void insertNewLine(String text);
	void insertTextAtCaret (const String &textToInsert);
	void insertMultiLineTextAtCaret (String text);
	void insertText(String text);
	
	void highlightLines(int firstLine, int lastLine);
	void highlightLine(int lineNumber);
	
	void handleAutoComplete(String text);
	void showAutoComplete(String currentWord);
	bool deleteForwards (const bool moveInWholeWordSteps);
	bool deleteBackwards (const bool moveInWholeWordSteps);
	
	
	void setAllText(String text)
	{
		getDocument().replaceAllContent(text);
	}


	void setOpcodeStrings(String opcodes)
    {
        opcodeStrings.addLines(opcodes);
    }
	
    int getNumRows()
    {
        return variableNamesToShow.size();
    }

    void listBoxItemDoubleClicked(int row, const MouseEvent &e) {};

    void paintListBoxItem (int rowNumber, Graphics& g,
                           int width, int height, bool rowIsSelected)
    {
        if (rowIsSelected)
            g.fillAll (Colours::whitesmoke.darker(.2));
        else
            g.fillAll(Colours::whitesmoke);

        g.setColour(Colour(20, 20, 20));
        g.drawFittedText(variableNamesToShow[rowNumber], Rectangle<int> (width, height), Justification::centredLeft, 0);
    }

    void selectedRowsChanged (int /*lastRowselected*/) {};	
private:
	Component* statusBar;
	int listBoxRowHeight = 18;
	StringArray opcodeStrings;
	bool columnEditMode = false;
	ListBox autoCompleteListBox;
	StringArray variableNamesToShow, variableNames;
};



#endif  // CABBAGECODEEDITOR_H_INCLUDED
