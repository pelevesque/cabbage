/*
  ==============================================================================

    CabbageSettings.h
    Created: 12 Oct 2016 12:12:47pm
    Author:  rory

  ==============================================================================
*/

#ifndef CABBAGESETTINGS_H_INCLUDED
#define CABBAGESETTINGS_H_INCLUDED


#include "../CabbageIds.h"

class CabbageSettings : public ApplicationProperties, public ValueTree::Listener, public ChangeBroadcaster
{
public:
    CabbageSettings();
    ~CabbageSettings() {};

    void set(String child, Identifier identifier, var value);
    String get(String child, String identifier);
    static Colour getColourFromValueTree(ValueTree valueTree, Identifier identifier, Colour defaultColour);
    static String getColourPropertyName(ValueTree valueTree, int index);
	int getIntProperty(String child);
    static Colour getColourFromValueTree(ValueTree valueTree, int index, Colour defaultColour);
    static void set(ValueTree tree, String child, Identifier identifier, var value);
    static String get(ValueTree tree, String child, String identifier);
	void setProperty(String child, var value);
	void updateRecentFilesList();
	void updateRecentFilesList(File file);
    int getIndexOfProperty(String child, String identifier);
    ValueTree getValueTree();
    XmlElement* getXML(String identifier);
    void setDefaultSettings();
    void setDefaultColourSettings();
	ScopedPointer<PropertySet> defaultPropSet;	
    ValueTree valueTree;
	RecentlyOpenedFilesList recentFiles;
	File getMostRecentFile();
	
	String getAudioSettingsXml()
	{
		return audioSettingsXml;
	}
	
private:


	String audioSettingsXml;
    void changed()
    {
        ScopedPointer<XmlElement> data (valueTree.createXml());
        getUserSettings()->setValue ("PROJECT_DEFAULT_SETTINGS", data);
        sendChangeMessage();
        XmlElement * el = valueTree.createXml();
        el->writeToFile(File("/home/rory/Desktop/Example1.xml"), String::empty);
        delete el;
    }

    //==============================================================================
    void valueTreePropertyChanged (ValueTree&, const Identifier&) override;
	
    void valueTreeChildAdded (ValueTree&, ValueTree&) override
    {
        changed();
    }
    void valueTreeChildRemoved (ValueTree&, ValueTree&, int) override
    {
        changed();
    }
    void valueTreeChildOrderChanged (ValueTree&, int, int) override
    {
        changed();
    }
    void valueTreeParentChanged (ValueTree&) override
    {
        changed();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CabbageSettings)
};



#endif  // CABBAGESETTINGS_H_INCLUDED