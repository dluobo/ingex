/*
 * $Id: XMLWriter.h,v 1.1 2010/06/02 10:52:38 philipn Exp $
 *
 * Copyright (C) 2010  British Broadcasting Corporation.
 * All Rights Reserved.
 *
 * Author: Philip de Nier
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef __XML_WRITER_H__
#define __XML_WRITER_H__

#include <cstdio>

#include <vector>
#include <map>
#include <string>



class XMLWriter
{
public:
    static XMLWriter* Open(std::string filename);
    
public:
    XMLWriter(FILE *xml_file);
    virtual ~XMLWriter();

    void WriteDocumentStart();
    void WriteDocumentEnd();
    void WriteElementStart(std::string ns, std::string local_name);
    void DeclareNamespace(std::string ns, std::string prefix);
    void WriteAttribute(std::string ns, std::string local_name, std::string value);
    void WriteAttributeStart(std::string ns, std::string local_name);
    void WriteAttributeContent(std::string value);
    void WriteAttributeEnd();
    void WriteElementContent(std::string content);
    void WriteElementEnd();
    void WriteComment(std::string comment);
    void WriteProcInstruction(std::string target, std::string instruction);
    void WriteText(std::string text);

    void Flush();

private:
    std::string GetPrefix(std::string ns);
    std::string GetNonDefaultNSPrefix(std::string ns);

    class Element
    {
    public:
        Element(Element* parentElement, std::string ns, std::string local_name);
        ~Element();

        bool AddNamespaceDecl(std::string ns, std::string prefix);
        std::map<std::string, std::string>& GetNamespaceDecls() { return mNSpaceDecls; }

        const std::string& GetNamespace() const { return mNS; }
        const std::string& GetLocalName() const { return mLocalName; }
        std::string GetPrefix(const std::string &ns) const;
        std::string GetNonDefaultNSPrefix(const std::string &ns) const;
        const std::string& GetPrefix() const { return mPrefix; }
        const std::string& GetDefaultNamespace() const { return mDefaultNS; }

    private:
        Element *mParentElement;
        std::string mNS;
        std::string mPrefix;
        std::string mLocalName;
        std::string mDefaultNS;
        std::map<std::string, std::string> mNSpaceDecls;
    };

    enum WriteType
    {
        NONE,
        START,
        END,
        ELEMENT_START,
        DELAYED_ELEMENT_START,
        ATTRIBUTE_START,
        ATTRIBUTE_CONTENT,
        ATTRIBUTE_END,
        ELEMENT_CONTENT,
        ELEMENT_END,
        COMMENT,
        PROC_INSTRUCTION
    };

    WriteType mPrevWriteType;
    std::vector<Element*> mElementStack;
    int mLevel;


private:
    void WriteProlog();
    void WriteElementData(const std::string &data);
    void WriteAttributeData(const std::string &data);
    void WriteIndent(int level);

    void Write(const std::string &data);
    void Write(const char *data);
    void Write(const char *data, size_t len);

private:
    FILE *mXMLFile;
};



#endif
