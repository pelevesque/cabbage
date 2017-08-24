/*
  Copyright (C) 2016 Rory Walsh

  Cabbage is free software; you can redistribute it
  and/or modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  Cabbage is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU General Public
  License along with Csound; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
  02111-1307 USA
*/

#include "CabbagePluginProcessor.h"
#include "CabbagePluginEditor.h"

char tmp_string[4096] = {0};
char channelMessage[4096] = {0};

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    File csdFile;
#ifndef JUCE_MAC
    csdFile = File::getSpecialLocation (File::currentExecutableFile).withFileExtension (String (".csd")).getFullPathName();
#else
    //read .csd file from the correct location within the .vst bundle.
    const String dir = File::getSpecialLocation (File::currentExecutableFile).getParentDirectory().getParentDirectory().getFullPathName();
    const String filename (File::getSpecialLocation (File::currentExecutableFile).withFileExtension (String (".csd")).getFileName());
    csdFile = File (dir + "/" + filename);

#endif
    return new CabbagePluginProcessor (csdFile);
};

//============================================================================
CabbagePluginProcessor::CabbagePluginProcessor (File inputFile)
    : CsoundPluginProcessor (inputFile),
      csdFile (inputFile),
      cabbageWidgets ("CabbageWidgetData")
{

    //initAllCsoundChannels(cabbageWidgets);
    if (inputFile.existsAsFile())
    {
        parseCsdFile (inputFile.loadFileAsString());
        createParameters();
    }

    initAllCsoundChannels (cabbageWidgets);
}

CabbagePluginProcessor::~CabbagePluginProcessor()
{
    for ( auto xyAuto : xyAutomators)
        xyAuto->removeAllChangeListeners();

    xyAutomators.clear();
}

//==============================================================================
void CabbagePluginProcessor::parseCsdFile (String csdText)
{
    StringArray linesFromCsd;
    cabbageWidgets.removeAllChildren (0);
    linesFromCsd.addLines (csdText);
    String parentComponent, previousComponent;

    searchForMacros (linesFromCsd);


    for ( int lineNumber = 0; lineNumber < linesFromCsd.size() ; lineNumber++ )
    {
        if (linesFromCsd[lineNumber].equalsIgnoreCase ("</Cabbage>"))
            return;

        const String widgetTreeIdentifier = "WidgetFromLine_" + String (lineNumber);
        ValueTree tempWidget (widgetTreeIdentifier);

        String currentLineOfCabbageCode = linesFromCsd[lineNumber];

        if (currentLineOfCabbageCode.contains (" \\"))
        {
            for (int index = lineNumber + 1;; index++)
            {

                if (linesFromCsd[index].contains (" \\"))
                    currentLineOfCabbageCode += linesFromCsd[index];
                else
                {
                    currentLineOfCabbageCode += linesFromCsd[index];
                    break;
                }

            }
        }

        const String expandedMacroText = getExpandedMacroText (currentLineOfCabbageCode, tempWidget);

        if ( currentLineOfCabbageCode.indexOf (";") > -1)
            currentLineOfCabbageCode = currentLineOfCabbageCode.substring (0, currentLineOfCabbageCode.indexOf (";"));

        const String comments = currentLineOfCabbageCode.indexOf (";") == -1 ? "" : currentLineOfCabbageCode.substring (currentLineOfCabbageCode.indexOf (";"));
        CabbageWidgetData::setWidgetState (tempWidget, currentLineOfCabbageCode.trimCharactersAtStart (" \t") + " " + expandedMacroText + comments, lineNumber);
        CabbageWidgetData::setNumProp (tempWidget, CabbageIdentifierIds::linenumber, lineNumber);
        CabbageWidgetData::setStringProp (tempWidget, CabbageIdentifierIds::csdfile, csdFile.getFullPathName());
        CabbageWidgetData::setStringProp (tempWidget, CabbageIdentifierIds::expandedmacrotext, expandedMacroText);

        if (CabbageWidgetData::getStringProp (tempWidget, CabbageIdentifierIds::type) == CabbageWidgetTypes::form)
        {
            const String caption = CabbageWidgetData::getStringProp (tempWidget, CabbageIdentifierIds::caption);
            this->setPluginName (caption.length() > 0 ? caption : "Untitled");

            if (CabbageWidgetData::getNumProp (tempWidget, CabbageIdentifierIds::logger) == 1)
                createFileLogger (this->csdFile);
        }

        if (currentLineOfCabbageCode.contains ("}"))
        {
            parentComponent = "";
        }

        if (parentComponent.isNotEmpty())
            CabbageWidgetData::setStringProp (tempWidget, CabbageIdentifierIds::parentcomponent, parentComponent);

		const String widgetName = CabbageWidgetData::getStringProp(tempWidget, CabbageIdentifierIds::name);

		if(widgetName.isNotEmpty())
			cabbageWidgets.addChild (tempWidget, -1, 0);

		if (CabbageWidgetData::getProperty (tempWidget, CabbageIdentifierIds::widgetarray).size() > 0 &&
			CabbageWidgetData::getProperty (tempWidget, CabbageIdentifierIds::identchannelarray).size() > 0)
		{
			for (int i = 0; i < CabbageWidgetData::getProperty (tempWidget, CabbageIdentifierIds::widgetarray).size(); i++)
			{
				ValueTree copy = tempWidget.createCopy();
				const String chan = CabbageWidgetData::getProperty (tempWidget, CabbageIdentifierIds::widgetarray)[i].toString();
				CabbageWidgetData::setStringProp (copy, CabbageIdentifierIds::channel, CabbageWidgetData::getProperty (tempWidget, CabbageIdentifierIds::widgetarray)[i]);
				CabbageWidgetData::setStringProp (copy, CabbageIdentifierIds::identchannel, CabbageWidgetData::getProperty (tempWidget, CabbageIdentifierIds::identchannelarray)[i]);
				cabbageWidgets.addChild (copy, -1, 0);
			}
		}


        if (currentLineOfCabbageCode.contains ("{"))
        {
            if (currentLineOfCabbageCode.removeCharacters (" ") == "{")
            {
                parentComponent = previousComponent;
            }
            else
            {
                parentComponent = CabbageWidgetData::getProperty (tempWidget, CabbageIdentifierIds::name).toString();
                CabbageWidgetData::setProperty (tempWidget, "containsOpeningCurlyBracket", 1);
            }
        }

        previousComponent = CabbageWidgetData::getProperty (tempWidget, CabbageIdentifierIds::name).toString();
    }
}

void CabbagePluginProcessor::searchForMacros (StringArray& linesFromCsd)
{
    for (String csdLine : linesFromCsd) //deal with Cabbage macros
    {
        StringArray tokens;
        csdLine = csdLine.replace ("\n", " ");
        tokens.addTokens (csdLine, ", ");

        if (tokens[0].containsIgnoreCase ("define"))
        {
            tokens.removeEmptyStrings();

            if (tokens.size() > 1)
            {
                const String currentMacroText = csdLine.substring (csdLine.indexOf (tokens[1]) + tokens[1].length()) + " ";
                //first identifiers are not being used for some reason. This hack fixes that, but should be tidied up..
				macroText.set ("$" + tokens[1], " " + currentMacroText + currentMacroText );
            }
        }
    }
}

const String CabbagePluginProcessor::getExpandedMacroText (const String line, ValueTree wData)
{
    String csdLine;
    var macroNames;

    for (int cnt = 0 ; cnt < macroText.size() ; cnt++)
    {
        if (line.contains (macroText.getName (cnt).toString()))
        {
            csdLine += macroText.getWithDefault (macroText.getName (cnt), "").toString() + " ";
            macroNames.append (macroText.getName (cnt).toString());
        }
    }

    CabbageWidgetData::setProperty (wData, CabbageIdentifierIds::macronames, macroNames);

    return csdLine;
}

//rebuild the entire GUi each time something changes.
void CabbagePluginProcessor::updateWidgets (String csdText)
{
    CabbagePluginEditor* editor = static_cast<CabbagePluginEditor*> (this->getActiveEditor());
    parseCsdFile (csdText);
    editor->createEditorInterface (cabbageWidgets);
    editor->updateLayoutEditorFrames();
}
//==============================================================================
// create parameters for sliders, buttons, comboboxes, checkboxes, encoders and xypads.
// Other widgets can communicate with Csound, but they cannot be automated
void CabbagePluginProcessor::createParameters()
{

    CabbageControlWidgetStrings controlWidgetTypes;

    for (int i = 0; i < cabbageWidgets.getNumChildren(); i++)
    {
        const String typeOfWidget = CabbageWidgetData::getStringProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::type);
        CabbageControlWidgetStrings controlWidgetTypes;

        if (controlWidgetTypes.contains (typeOfWidget))
        {
            const String name = CabbageWidgetData::getStringProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::name);
            const String channel = CabbageWidgetData::getStringProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::channel);
            const var value = CabbageWidgetData::getProperty (cabbageWidgets.getChild (i), CabbageIdentifierIds::value);

            if (controlWidgetTypes.contains (CabbageWidgetData::getStringProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::type)))
            {

                if (typeOfWidget == CabbageWidgetTypes::xypad)
                {
                    const var channel = CabbageWidgetData::getProperty (cabbageWidgets.getChild (i), CabbageIdentifierIds::channel);
					const float increment = CabbageWidgetData::getNumProp(cabbageWidgets.getChild(i), CabbageIdentifierIds::increment);					
                    addParameter (new CabbageAudioParameter (cabbageWidgets.getChild (i), *getCsound(), channel[0] , name + "_x", 0, 1, value, increment, 1));
                    addParameter (new CabbageAudioParameter (cabbageWidgets.getChild (i), *getCsound(), channel[1], name + "_y", 0, 1, value, increment, 1));
                }

                else if (typeOfWidget.contains ("range"))
                {
                    const var channel = CabbageWidgetData::getProperty (cabbageWidgets.getChild (i), CabbageIdentifierIds::channel);
					if (channel.size() > 1)
					{
						const float increment = CabbageWidgetData::getNumProp(cabbageWidgets.getChild(i), CabbageIdentifierIds::increment);
						const int minValue = CabbageWidgetData::getNumProp(cabbageWidgets.getChild(i), CabbageIdentifierIds::minvalue);
						const int maxValue = CabbageWidgetData::getNumProp(cabbageWidgets.getChild(i), CabbageIdentifierIds::maxvalue);
						const float skew = CabbageWidgetData::getNumProp(cabbageWidgets.getChild(i), CabbageIdentifierIds::sliderskew);
						const float min = CabbageWidgetData::getNumProp(cabbageWidgets.getChild(i), CabbageIdentifierIds::min);
						const float max = CabbageWidgetData::getNumProp(cabbageWidgets.getChild(i), CabbageIdentifierIds::max);
						addParameter(new CabbageAudioParameter(cabbageWidgets.getChild(i), *getCsound(), channel[0], name + "_min", min, max, minValue, increment, skew));
						addParameter(new CabbageAudioParameter(cabbageWidgets.getChild(i), *getCsound(), channel[1], name + "_max", min, max, maxValue, increment, skew));
					}
                }
				else if(typeOfWidget == CabbageWidgetTypes::combobox && channel.isNotEmpty())
				{
					const float min = CabbageWidgetData::getNumProp(cabbageWidgets.getChild(i), CabbageIdentifierIds::min);
					const float max = CabbageWidgetData::getNumProp(cabbageWidgets.getChild(i), CabbageIdentifierIds::comborange);
					addParameter (new CabbageAudioParameter (cabbageWidgets.getChild (i), *getCsound(), channel, name, min, max, value, 1, 1));
				}
				else if(typeOfWidget.contains("slider") && channel.isNotEmpty())
				{
					const float increment = CabbageWidgetData::getNumProp(cabbageWidgets.getChild(i), CabbageIdentifierIds::increment);
					const float skew = CabbageWidgetData::getNumProp(cabbageWidgets.getChild(i), CabbageIdentifierIds::sliderskew);
					const float min = CabbageWidgetData::getNumProp(cabbageWidgets.getChild(i), CabbageIdentifierIds::min);
					const float max = CabbageWidgetData::getNumProp(cabbageWidgets.getChild(i), CabbageIdentifierIds::max);
					addParameter (new CabbageAudioParameter (cabbageWidgets.getChild (i), *getCsound(), channel, name, min, max, value, increment, skew));
				}
                else
                {
					if(channel.isNotEmpty())
						addParameter (new CabbageAudioParameter (cabbageWidgets.getChild (i), *getCsound(), channel, name, 0, 1, value, 1, 1));
                }
            }
        }
    }
}

//==============================================================================
bool CabbagePluginProcessor::hasEditor() const
{
    return true;
}

AudioProcessorEditor* CabbagePluginProcessor::createEditor()
{
    return new CabbagePluginEditor (*this);
}

//==============================================================================
void CabbagePluginProcessor::getStateInformation (MemoryBlock& destData)
{
    copyXmlToBinary (savePluginState ("CABBAGE_PLUGIN_SETTINGS"), destData);
}

void CabbagePluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    ScopedPointer<XmlElement> xmlElement = getXmlFromBinary (data, sizeInBytes);
    restorePluginState (xmlElement);
}

//==============================================================================
XmlElement CabbagePluginProcessor::savePluginState (String xmlTag)
{
    XmlElement xml (xmlTag);

    for (int i = 0 ; i < cabbageWidgets.getNumChildren() ; i++)
    {
        const String channelName = CabbageWidgetData::getStringProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::channel);
		//const String widgetName = CabbageWidgetData::getStringProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::name);
		
        const String type = CabbageWidgetData::getStringProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::type);
        const var value = CabbageWidgetData::getProperty (cabbageWidgets.getChild (i), CabbageIdentifierIds::value);

		//only write values for widgets that have channels
		if(channelName.isNotEmpty())
		{
			if (type == CabbageWidgetTypes::texteditor)
			{
				const String text = CabbageWidgetData::getStringProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::text);
				xml.setAttribute (channelName, text);
			}
			else if (type == CabbageWidgetTypes::filebutton && !CabbageWidgetData::getStringProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::filetype).contains("snaps"))
			{
				const String file = CabbageWidgetData::getStringProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::file);
				if(file.length()>2)
				{
					const String relativePath = File (file).getRelativePathFrom (File (csdFile));
					xml.setAttribute (channelName, relativePath);
				}
			}
			else if(type.contains("range"))//double channel range widgets
			{
				var channels = CabbageWidgetData::getProperty (cabbageWidgets.getChild (i), CabbageIdentifierIds::channel);
				const float minValue = CabbageWidgetData::getNumProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::minvalue);
				const float maxValue = CabbageWidgetData::getNumProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::maxvalue);
				xml.setAttribute (channels[0].toString(), minValue);
				xml.setAttribute (channels[1].toString(), maxValue);
			}
			else if(type == CabbageWidgetTypes::xypad)//double channel xypad widget
			{
				var channels = CabbageWidgetData::getProperty (cabbageWidgets.getChild (i), CabbageIdentifierIds::channel);
				const float xValue = CabbageWidgetData::getNumProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::valuex);
				const float yValue = CabbageWidgetData::getNumProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::valuey);
				xml.setAttribute (channels[0].toString(), xValue);
				xml.setAttribute (channels[1].toString(), yValue);
			}
			else
				xml.setAttribute (channelName, float(value));
		}
    }

    return xml;
}

void CabbagePluginProcessor::restorePluginState (XmlElement* xmlState)
{
    if (xmlState != nullptr)
    {
        for (int i = 0; i < xmlState->getNumAttributes(); i++)
        {
            ValueTree valueTree = CabbageWidgetData::getValueTreeForComponent (cabbageWidgets, xmlState->getAttributeName (i), true);
            const String type = CabbageWidgetData::getStringProp (valueTree, CabbageIdentifierIds::type);

            if (type == CabbageWidgetTypes::texteditor)
                CabbageWidgetData::setStringProp (valueTree, CabbageIdentifierIds::text, xmlState->getAttributeValue (i));
            else if (type == CabbageWidgetTypes::filebutton)
            {
				CabbageUtilities::debug(xmlState->getAttributeValue (i));
                CabbageWidgetData::setStringProp (valueTree, CabbageIdentifierIds::file, xmlState->getAttributeValue (i));
            }
			else if(type == CabbageWidgetTypes::hrange || type == CabbageWidgetTypes::vrange)//double channel range widgets
			{
				CabbageWidgetData::setNumProp (valueTree, CabbageIdentifierIds::minvalue, xmlState->getAttributeValue (i).getFloatValue());
				CabbageWidgetData::setNumProp (valueTree, CabbageIdentifierIds::maxvalue, xmlState->getAttributeValue (i+1).getFloatValue());
				i++;
			}
			else if(type == CabbageWidgetTypes::xypad)//double channel range widgets
			{
				CabbageWidgetData::setNumProp (valueTree, CabbageIdentifierIds::valuex, xmlState->getAttributeValue (i).getFloatValue());
				CabbageWidgetData::setNumProp (valueTree, CabbageIdentifierIds::valuey, xmlState->getAttributeValue (i+1).getFloatValue());
				i++;
			}
            else
            {
                CabbageWidgetData::setNumProp (valueTree, CabbageIdentifierIds::value, xmlState->getAttributeValue (i).getFloatValue());
            }
        }

        initAllCsoundChannels (cabbageWidgets);
    }

    xmlState = nullptr;
}

//==============================================================================
void CabbagePluginProcessor::getChannelDataFromCsound()
{
    for ( int i = 0; i < cabbageWidgets.getNumChildren(); i++)
    {
        const var chanArray = CabbageWidgetData::getProperty (cabbageWidgets.getChild (i), CabbageIdentifierIds::channel);
        const var widgetArray = CabbageWidgetData::getProperty (cabbageWidgets.getChild (i), CabbageIdentifierIds::widgetarray);
        
		StringArray channels;

		if(widgetArray.size()>0)
			channels.add (CabbageWidgetData::getStringProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::channel));
        else if (chanArray.size() == 1)
            channels.add (CabbageWidgetData::getStringProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::channel));
        else if(chanArray.size() > 1)
        {
            for (int j = 0; j < chanArray.size(); j++)
                channels.add (var (chanArray[j]));
        }

        const var value = CabbageWidgetData::getProperty (cabbageWidgets.getChild (i), CabbageIdentifierIds::value);
        const float valuex = CabbageWidgetData::getNumProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::valuex);
        const float valuey = CabbageWidgetData::getNumProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::valuey);
        const String identChannel = CabbageWidgetData::getStringProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::identchannel);
        const String identChannelMessage = CabbageWidgetData::getStringProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::identchannelmessage);
        const String typeOfWidget = CabbageWidgetData::getStringProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::type);

		const String chann = channels[0];

		
		if (channels.size() == 1 && channels[0].isNotEmpty())
		{
				
					if (typeOfWidget != "combobox")	// don't update combobox in here, it will enter a recursive loop
					{
						if (getCsound()->GetChannel (channels[0].toUTF8()) != float(value))
							CabbageWidgetData::setNumProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::value, getCsound()->GetChannel (channels[0].toUTF8()));
					}
					else
					{
						CabbageWidgetData::setNumProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::update, 0);
						if(value.isString() == false)
						{
							if (getCsound()->GetChannel (channels[0].toUTF8()) != float(value))
							{
								CabbageWidgetData::setNumProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::value, getCsound()->GetChannel (channels[0].toUTF8()));
							}
						}
						else
						{
							char tmp_str[4096] = {0};
							getCsound()->GetStringChannel(channels[0].toUTF8(), tmp_str);
							CabbageWidgetData::setProperty (cabbageWidgets.getChild (i), CabbageIdentifierIds::value, String(tmp_str));
						}
					}
		}
		
		//currently only dealing with a max of 2 channels...
		else if(channels.size() == 2 && channels[0].isNotEmpty() && channels[1].isNotEmpty())
		{
			if(getCsound()->GetChannel (channels[0].toUTF8()) != valuex
				|| getCsound()->GetChannel (channels[1].toUTF8()) != valuey)
			{
				if(typeOfWidget == CabbageWidgetTypes::xypad)
				{
					CabbageWidgetData::setNumProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::valuex, getCsound()->GetChannel (channels[0].toUTF8()));
					CabbageWidgetData::setNumProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::valuey, getCsound()->GetChannel (channels[1].toUTF8()));
				}
				else if(typeOfWidget.contains("range"))
				{
					const float minValue = CabbageWidgetData::getNumProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::minvalue);
					const float maxValue = CabbageWidgetData::getNumProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::maxvalue);		
					CabbageWidgetData::setNumProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::minvalue, getCsound()->GetChannel (channels[0].toUTF8()));
					CabbageWidgetData::setNumProp (cabbageWidgets.getChild (i), CabbageIdentifierIds::maxvalue, getCsound()->GetChannel (channels[1].toUTF8()));
				}			
			}	
		}

			
        if (identChannel.isNotEmpty())
        {
            getCsound()->GetStringChannel (identChannel.toUTF8(), tmp_string);
            String identifierText (tmp_string);

            if (identifierText != identChannelMessage)
            {
                CabbageWidgetData::setCustomWidgetState (cabbageWidgets.getChild (i), " " + String (tmp_string));

                if (identifierText.contains ("tablenumber")) //update even if table number has not changed
                    CabbageWidgetData::setProperty (cabbageWidgets.getChild (i), CabbageIdentifierIds::update, 1);

                getCsound()->SetChannel (identChannel.toUTF8(), (char*)"");
            }
        }
    }
}

//================================================================================
void CabbagePluginProcessor::addXYAutomator (CabbageXYPad* xyPad, ValueTree wData)
{
    int indexOfAutomator = -1;

    for ( int i = 0 ; i < xyAutomators.size() ; i++ )
    {
        if (xyPad->getName() == xyAutomators[i]->getName())
            indexOfAutomator = i;
    }

    if (indexOfAutomator == -1)
    {
        XYPadAutomator* xyAuto;
        CabbageAudioParameter* xParameter = getParameterForXYPad (xyPad->getName() + "_x");
        CabbageAudioParameter* yParameter = getParameterForXYPad (xyPad->getName() + "_y");

        if (xParameter && yParameter)
        {
            xyAutomators.add (xyAuto = new XYPadAutomator (xyPad->getName(), xParameter, yParameter, this));
            xyAuto->setXMin (CabbageWidgetData::getNumProp (wData, CabbageIdentifierIds::minx));
            xyAuto->setYMin (CabbageWidgetData::getNumProp (wData, CabbageIdentifierIds::miny));
            xyAuto->setXMax (CabbageWidgetData::getNumProp (wData, CabbageIdentifierIds::maxx));
            xyAuto->setYMax (CabbageWidgetData::getNumProp (wData, CabbageIdentifierIds::maxy));
            xyAuto->addChangeListener (xyPad);
        }
    }
    else
    {
        xyAutomators[indexOfAutomator]->addChangeListener (xyPad);
    }
}


void CabbagePluginProcessor::enableXYAutomator (String name, bool enable, Line<float> dragLine)
{

    for ( XYPadAutomator* xyAuto : xyAutomators)
    {
        if (name == xyAuto->getName())
        {
            if (enable == true)
            {
                xyAuto->setDragLine (dragLine);
                xyAuto->setXValue (dragLine.getEndX());
                xyAuto->setYValue (dragLine.getEndY());
                xyAuto->setXValueIncrement ((dragLine.getEndX() - dragLine.getStartX())*.05);
                xyAuto->setYValueIncrement ((dragLine.getEndY() - dragLine.getStartY())*.05);
                xyAuto->setRepaintBackground (true);
                xyAuto->setIsPluginEditorOpen (getActiveEditor() != nullptr ? true : false);
                xyAuto->startTimer (20);
            }
            else
                xyAuto->stopTimer();
        }
    }
}

//======================================================================================================
CabbageAudioParameter* CabbagePluginProcessor::getParameterForXYPad (String name)
{
    for (auto param : getParameters())
    {
        if (CabbageAudioParameter* cabbageParam = dynamic_cast<CabbageAudioParameter*> (param))
        {
            if (name == cabbageParam->getWidgetName())
                return dynamic_cast<CabbageAudioParameter*> (cabbageParam);

        }
    }

    return nullptr;
}











