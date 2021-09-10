#ifndef CONF_MANAGER_HPP
#define CONF_MANAGER_HPP

// Includes
#include <iostream>
#include <sstream>
#include <string>
#include <locale>
#include <sstream>
#include <vector>
#include <fstream>  
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")
#else
#include <unistd.h>
#include <sys/vfs.h>
#endif



//https://github.com/brofield/simpleini
/*The MIT License (MIT)

Copyright (c) 2006-2013 Brodie Thiesfield

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.*/

#ifndef INCLUDED_SimpleIni_h
#define INCLUDED_SimpleIni_h

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

// Disable these warnings in MSVC:
//  4127 "conditional expression is constant" as the conversion classes trigger
//  it with the statement if (sizeof(SI_CHAR) == sizeof(char)). This test will
//  be optimized away in a release build.
//  4503 'insert' : decorated name length exceeded, name was truncated
//  4702 "unreachable code" as the MS STL header causes it in release mode.
//  Again, the code causing the warning will be cleaned up by the compiler.
//  4786 "identifier truncated to 256 characters" as this is thrown hundreds
//  of times VC6 as soon as STL is used.
#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable: 4127 4503 4702 4786)
#endif

#include <cstring>
#include <cstdlib>
#include <string>
#include <map>
#include <list>
#include <algorithm>
#include <stdio.h>

#ifdef SI_SUPPORT_IOSTREAMS
#include <iostream>
#endif // SI_SUPPORT_IOSTREAMS

#ifdef _DEBUG
#ifndef assert
#include <cassert>
#endif
#define SI_ASSERT(x)   assert(x)
#else
#define SI_ASSERT(x)
#endif

namespace sl_oc {
namespace tools {

enum SI_Error {
    SI_OK = 0, //!< No error
    SI_UPDATED = 1, //!< An existing value was updated
    SI_INSERTED = 2, //!< A new value was inserted

    // note: test for any error with (retval < 0)
    SI_FAIL = -1, //!< Generic failure
    SI_NOMEM = -2, //!< Out of memory error
    SI_FILE = -3 //!< File error (see errno for detail error)
};

#define SI_UTF8_SIGNATURE     "\xEF\xBB\xBF"

#ifdef _WIN32
#define SI_NEWLINE_A   "\r\n"
#define SI_NEWLINE_W   L"\r\n"
#else // !_WIN32
#define SI_NEWLINE_A   "\n"
#define SI_NEWLINE_W   L"\n"
#endif // _WIN32

#if defined(SI_CONVERT_ICU)
#include <unicode/ustring.h>
#endif

#if defined(_WIN32)
#define SI_HAS_WIDE_FILE
#define SI_WCHAR_T     wchar_t
#elif defined(SI_CONVERT_ICU)
#define SI_HAS_WIDE_FILE
#define SI_WCHAR_T     UChar
#endif

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
class CSimpleIniTempl {
public:
    typedef SI_CHAR SI_CHAR_T;

    /** key entry */
    struct Entry {
        const SI_CHAR * pItem;
        const SI_CHAR * pComment;
        int nOrder;

        Entry(const SI_CHAR * a_pszItem = NULL, int a_nOrder = 0)
        : pItem(a_pszItem)
        , pComment(NULL)
        , nOrder(a_nOrder) {
        }

        Entry(const SI_CHAR * a_pszItem, const SI_CHAR * a_pszComment, int a_nOrder)
        : pItem(a_pszItem)
        , pComment(a_pszComment)
        , nOrder(a_nOrder) {
        }

        Entry(const Entry & rhs) {
            operator=(rhs);
        }

        Entry & operator=(const Entry & rhs) {
            pItem = rhs.pItem;
            pComment = rhs.pComment;
            nOrder = rhs.nOrder;
            return *this;
        }

#if defined(_MSC_VER) && _MSC_VER <= 1200

        /** STL of VC6 doesn't allow me to specify my own comparator for list::sort() */
        bool operator<(const Entry & rhs) const {
            return LoadOrder()(*this, rhs);
        }

        bool operator>(const Entry & rhs) const {
            return LoadOrder()(rhs, *this);
        }
#endif

        /** Strict less ordering by name of key only */
        struct KeyOrder : std::binary_function<Entry, Entry, bool> {

            bool operator()(const Entry & lhs, const Entry & rhs) const {
                const static SI_STRLESS isLess = SI_STRLESS();
                return isLess(lhs.pItem, rhs.pItem);
            }
        };

        /** Strict less ordering by order, and then name of key */
        struct LoadOrder : std::binary_function<Entry, Entry, bool> {

            bool operator()(const Entry & lhs, const Entry & rhs) const {
                if (lhs.nOrder != rhs.nOrder) {
                    return lhs.nOrder < rhs.nOrder;
                }
                return KeyOrder()(lhs.pItem, rhs.pItem);
            }
        };
    };

    /** map keys to values */
    typedef std::multimap<Entry, const SI_CHAR *, typename Entry::KeyOrder> TKeyVal;

    /** map sections to key/value map */
    typedef std::map<Entry, TKeyVal, typename Entry::KeyOrder> TSection;

    /** set of dependent string pointers. Note that these pointers are
        dependent on memory owned by CSimpleIni.
     */
    typedef std::list<Entry> TNamesDepend;

    /** interface definition for the OutputWriter object to pass to Save()
        in order to output the INI file data.
     */
    class OutputWriter {
    public:

        OutputWriter() {
        }

        virtual ~OutputWriter() {
        }
        virtual void Write(const char * a_pBuf) = 0;
    private:
        OutputWriter(const OutputWriter &); // disable
        OutputWriter & operator=(const OutputWriter &); // disable
    };

    /** OutputWriter class to write the INI data to a file */
    class FileWriter : public OutputWriter {
        FILE * m_file;
    public:

        FileWriter(FILE * a_file) : m_file(a_file) {
        }

        void Write(const char * a_pBuf) {
            fputs(a_pBuf, m_file);
        }
    private:
        FileWriter(const FileWriter &); // disable
        FileWriter & operator=(const FileWriter &); // disable
    };

    /** OutputWriter class to write the INI data to a string */
    class StringWriter : public OutputWriter {
        std::string & m_string;
    public:

        StringWriter(std::string & a_string) : m_string(a_string) {
        }

        void Write(const char * a_pBuf) {
            m_string.append(a_pBuf);
        }
    private:
        StringWriter(const StringWriter &); // disable
        StringWriter & operator=(const StringWriter &); // disable
    };

#ifdef SI_SUPPORT_IOSTREAMS

    /** OutputWriter class to write the INI data to an ostream */
    class StreamWriter : public OutputWriter {
        std::ostream & m_ostream;
    public:

        StreamWriter(std::ostream & a_ostream) : m_ostream(a_ostream) {
        }

        void Write(const char * a_pBuf) {
            m_ostream << a_pBuf;
        }
    private:
        StreamWriter(const StreamWriter &); // disable
        StreamWriter & operator=(const StreamWriter &); // disable
    };
#endif // SI_SUPPORT_IOSTREAMS

    /** Characterset conversion utility class to convert strings to the
        same format as is used for the storage.
     */
    class Converter : private SI_CONVERTER {
    public:

        Converter(bool a_bStoreIsUtf8) : SI_CONVERTER(a_bStoreIsUtf8) {
            m_scratch.resize(1024);
        }

        Converter(const Converter & rhs) {
            operator=(rhs);
        }

        Converter & operator=(const Converter & rhs) {
            m_scratch = rhs.m_scratch;
            return *this;
        }

        bool ConvertToStore(const SI_CHAR * a_pszString) {
            size_t uLen = SI_CONVERTER::SizeToStore(a_pszString);
            if (uLen == (size_t) (-1)) {
                return false;
            }
            while (uLen > m_scratch.size()) {
                m_scratch.resize(m_scratch.size() * 2);
            }
            return SI_CONVERTER::ConvertToStore(
                    a_pszString,
                    const_cast<char*> (m_scratch.data()),
                    m_scratch.size());
        }

        const char * Data() {
            return m_scratch.data();
        }
    private:
        std::string m_scratch;
    };

public:
    /*-----------------------------------------------------------------------*/

    /** Default constructor.

        @param a_bIsUtf8     See the method SetUnicode() for details.
        @param a_bMultiKey   See the method SetMultiKey() for details.
        @param a_bMultiLine  See the method SetMultiLine() for details.
     */
    CSimpleIniTempl(
            bool a_bIsUtf8 = false,
            bool a_bMultiKey = false,
            bool a_bMultiLine = false
            );

    /** Destructor */
    ~CSimpleIniTempl();

    /** Deallocate all memory stored by this object */
    void Reset();

    /** Has any data been loaded */
    bool IsEmpty() const {
        return m_data.empty();
    }

    /*-----------------------------------------------------------------------*/
    /** @{ @name Settings */

    /** Set the storage format of the INI data. This affects both the loading
        and saving of the INI data using all of the Load/Save API functions.
        This value cannot be changed after any INI data has been loaded.

        If the file is not set to Unicode (UTF-8), then the data encoding is
        assumed to be the OS native encoding. This encoding is the system
        locale on Linux/Unix and the legacy MBCS encoding on Windows NT/2K/XP.
        If the storage format is set to Unicode then the file will be loaded
        as UTF-8 encoded data regardless of the native file encoding. If
        SI_CHAR == char then all of the char* parameters take and return UTF-8
        encoded data regardless of the system locale.

        \param a_bIsUtf8     Assume UTF-8 encoding for the source?
     */
    void SetUnicode(bool a_bIsUtf8 = true) {
        if (!m_pData) m_bStoreIsUtf8 = a_bIsUtf8;
    }

    /** Get the storage format of the INI data. */
    bool IsUnicode() const {
        return m_bStoreIsUtf8;
    }

    /** Should multiple identical keys be permitted in the file. If set to false
        then the last value encountered will be used as the value of the key.
        If set to true, then all values will be available to be queried. For
        example, with the following input:

        <pre>
        [section]
        test=value1
        test=value2
        </pre>

        Then with SetMultiKey(true), both of the values "value1" and "value2"
        will be returned for the key test. If SetMultiKey(false) is used, then
        the value for "test" will only be "value2". This value may be changed
        at any time.

        \param a_bAllowMultiKey  Allow multi-keys in the source?
     */
    void SetMultiKey(bool a_bAllowMultiKey = true) {
        m_bAllowMultiKey = a_bAllowMultiKey;
    }

    /** Get the storage format of the INI data. */
    bool IsMultiKey() const {
        return m_bAllowMultiKey;
    }

    /** Should data values be permitted to span multiple lines in the file. If
        set to false then the multi-line construct <<<TAG as a value will be
        returned as is instead of loading the data. This value may be changed
        at any time.

        \param a_bAllowMultiLine     Allow multi-line values in the source?
     */
    void SetMultiLine(bool a_bAllowMultiLine = true) {
        m_bAllowMultiLine = a_bAllowMultiLine;
    }

    /** Query the status of multi-line data */
    bool IsMultiLine() const {
        return m_bAllowMultiLine;
    }

    /** Should spaces be added around the equals sign when writing key/value
        pairs out. When true, the result will be "key = value". When false,
        the result will be "key=value". This value may be changed at any time.

        \param a_bSpaces     Add spaces around the equals sign?
     */
    void SetSpaces(bool a_bSpaces = true) {
        m_bSpaces = a_bSpaces;
    }

    /** Query the status of spaces output */
    bool UsingSpaces() const {
        return m_bSpaces;
    }

    /*-----------------------------------------------------------------------*/
    /** @}
        @{ @name Loading INI Data */

    /** Load an INI file from disk into memory

        @param a_pszFile    Path of the file to be loaded. This will be passed
                            to fopen() and so must be a valid path for the
                            current platform.

        @return SI_Error    See error definitions
     */
    SI_Error LoadFile(
            const char * a_pszFile
            );

#ifdef SI_HAS_WIDE_FILE
    /** Load an INI file from disk into memory

        @param a_pwszFile   Path of the file to be loaded in UTF-16.

        @return SI_Error    See error definitions
     */
    SI_Error LoadFile(
            const SI_WCHAR_T * a_pwszFile
            );
#endif // SI_HAS_WIDE_FILE

    /** Load the file from a file pointer.

        @param a_fpFile     Valid file pointer to read the file data from. The
                            file will be read until end of file.

        @return SI_Error    See error definitions
     */
    SI_Error LoadFile(
            FILE * a_fpFile
            );

#ifdef SI_SUPPORT_IOSTREAMS
    /** Load INI file data from an istream.

        @param a_istream    Stream to read from

        @return SI_Error    See error definitions
     */
    SI_Error LoadData(
            std::istream & a_istream
            );
#endif // SI_SUPPORT_IOSTREAMS

    /** Load INI file data direct from a std::string

        @param a_strData    Data to be loaded

        @return SI_Error    See error definitions
     */
    SI_Error LoadData(const std::string & a_strData) {
        return LoadData(a_strData.c_str(), a_strData.size());
    }

    /** Load INI file data direct from memory

        @param a_pData      Data to be loaded
        @param a_uDataLen   Length of the data in bytes

        @return SI_Error    See error definitions
     */
    SI_Error LoadData(
            const char * a_pData,
            size_t a_uDataLen
            );

    /*-----------------------------------------------------------------------*/
    /** Save an INI file from memory to disk

        @param a_pszFile    Path of the file to be saved. This will be passed
                            to fopen() and so must be a valid path for the
                            current platform.

        @param a_bAddSignature  Prepend the UTF-8 BOM if the output data is
                            in UTF-8 format. If it is not UTF-8 then
                            this parameter is ignored.

        @return SI_Error    See error definitions
     */
    SI_Error SaveFile(
            const char * a_pszFile,
            bool a_bAddSignature = true
            ) const;

#ifdef SI_HAS_WIDE_FILE
    /** Save an INI file from memory to disk

        @param a_pwszFile   Path of the file to be saved in UTF-16.

        @param a_bAddSignature  Prepend the UTF-8 BOM if the output data is
                            in UTF-8 format. If it is not UTF-8 then
                            this parameter is ignored.

        @return SI_Error    See error definitions
     */
    SI_Error SaveFile(
            const SI_WCHAR_T * a_pwszFile,
            bool a_bAddSignature = true
            ) const;
#endif // _WIN32

    /** Save the INI data to a file. See Save() for details.

        @param a_pFile      Handle to a file. File should be opened for
                            binary output.

        @param a_bAddSignature  Prepend the UTF-8 BOM if the output data is in
                            UTF-8 format. If it is not UTF-8 then this value is
                            ignored. Do not set this to true if anything has
                            already been written to the file.

        @return SI_Error    See error definitions
     */
    SI_Error SaveFile(
            FILE * a_pFile,
            bool a_bAddSignature = false
            ) const;

    /** Save the INI data. The data will be written to the output device
        in a format appropriate to the current data, selected by:

        <table>
            <tr><th>SI_CHAR     <th>FORMAT
            <tr><td>char        <td>same format as when loaded (MBCS or UTF-8)
            <tr><td>wchar_t     <td>UTF-8
            <tr><td>other       <td>UTF-8
        </table>

        Note that comments from the original data is preserved as per the
        documentation on comments. The order of the sections and values
        from the original file will be preserved.

        Any data prepended or appended to the output device must use the the
        same format (MBCS or UTF-8). You may use the GetConverter() method to
        convert text to the correct format regardless of the output format
        being used by SimpleIni.

        To add a BOM to UTF-8 data, write it out manually at the very beginning
        like is done in SaveFile when a_bUseBOM is true.

        @param a_oOutput    Output writer to write the data to.

        @param a_bAddSignature  Prepend the UTF-8 BOM if the output data is in
                            UTF-8 format. If it is not UTF-8 then this value is
                            ignored. Do not set this to true if anything has
                            already been written to the OutputWriter.

        @return SI_Error    See error definitions
     */
    SI_Error Save(
            OutputWriter & a_oOutput,
            bool a_bAddSignature = false
            ) const;

#ifdef SI_SUPPORT_IOSTREAMS

    /** Save the INI data to an ostream. See Save() for details.

        @param a_ostream    String to have the INI data appended to.

        @param a_bAddSignature  Prepend the UTF-8 BOM if the output data is in
                            UTF-8 format. If it is not UTF-8 then this value is
                            ignored. Do not set this to true if anything has
                            already been written to the stream.

        @return SI_Error    See error definitions
     */
    SI_Error Save(
            std::ostream & a_ostream,
            bool a_bAddSignature = false
            ) const {
        StreamWriter writer(a_ostream);
        return Save(writer, a_bAddSignature);
    }
#endif // SI_SUPPORT_IOSTREAMS

    /** Append the INI data to a string. See Save() for details.

        @param a_sBuffer    String to have the INI data appended to.

        @param a_bAddSignature  Prepend the UTF-8 BOM if the output data is in
                            UTF-8 format. If it is not UTF-8 then this value is
                            ignored. Do not set this to true if anything has
                            already been written to the string.

        @return SI_Error    See error definitions
     */
    SI_Error Save(
            std::string & a_sBuffer,
            bool a_bAddSignature = false
            ) const {
        StringWriter writer(a_sBuffer);
        return Save(writer, a_bAddSignature);
    }

    /*-----------------------------------------------------------------------*/
    /** Retrieve all section names. The list is returned as an STL vector of
        names and can be iterated or searched as necessary. Note that the
        sort order of the returned strings is NOT DEFINED. You can sort
        the names into the load order if desired. Search this file for ".sort"
        for an example.

        NOTE! This structure contains only pointers to strings. The actual
        string data is stored in memory owned by CSimpleIni. Ensure that the
        CSimpleIni object is not destroyed or Reset() while these pointers
        are in use!

        @param a_names          Vector that will receive all of the section
                                 names. See note above!
     */
    void GetAllSections(
            TNamesDepend & a_names
            ) const;

    /** Retrieve all unique key names in a section. The sort order of the
        returned strings is NOT DEFINED. You can sort the names into the load
        order if desired. Search this file for ".sort" for an example. Only
        unique key names are returned.

        NOTE! This structure contains only pointers to strings. The actual
        string data is stored in memory owned by CSimpleIni. Ensure that the
        CSimpleIni object is not destroyed or Reset() while these strings
        are in use!

        @param a_pSection       Section to request data for
        @param a_names          List that will receive all of the key
                                 names. See note above!

        @return true            Section was found.
        @return false           Matching section was not found.
     */
    bool GetAllKeys(
            const SI_CHAR * a_pSection,
            TNamesDepend & a_names
            ) const;

    /** Retrieve all values for a specific key. This method can be used when
        multiple keys are both enabled and disabled. Note that the sort order
        of the returned strings is NOT DEFINED. You can sort the names into
        the load order if desired. Search this file for ".sort" for an example.

        NOTE! The returned values are pointers to string data stored in memory
        owned by CSimpleIni. Ensure that the CSimpleIni object is not destroyed
        or Reset while you are using this pointer!

        @param a_pSection       Section to search
        @param a_pKey           Key to search for
        @param a_values         List to return if the key is not found

        @return true            Key was found.
        @return false           Matching section/key was not found.
     */
    bool GetAllValues(
            const SI_CHAR * a_pSection,
            const SI_CHAR * a_pKey,
            TNamesDepend & a_values
            ) const;

    /** Query the number of keys in a specific section. Note that if multiple
        keys are enabled, then this value may be different to the number of
        keys returned by GetAllKeys.

        @param a_pSection       Section to request data for

        @return -1              Section does not exist in the file
        @return >=0             Number of keys in the section
     */
    int GetSectionSize(
            const SI_CHAR * a_pSection
            ) const;

    /** Retrieve all key and value pairs for a section. The data is returned
        as a pointer to an STL map and can be iterated or searched as
        desired. Note that multiple entries for the same key may exist when
        multiple keys have been enabled.

        NOTE! This structure contains only pointers to strings. The actual
        string data is stored in memory owned by CSimpleIni. Ensure that the
        CSimpleIni object is not destroyed or Reset() while these strings
        are in use!

        @param a_pSection       Name of the section to return
        @return boolean         Was a section matching the supplied
                                name found.
     */
    const TKeyVal * GetSection(
            const SI_CHAR * a_pSection
            ) const;

    /** Retrieve the value for a specific key. If multiple keys are enabled
        (see SetMultiKey) then only the first value associated with that key
        will be returned, see GetAllValues for getting all values with multikey.

        NOTE! The returned value is a pointer to string data stored in memory
        owned by CSimpleIni. Ensure that the CSimpleIni object is not destroyed
        or Reset while you are using this pointer!

        @param a_pSection       Section to search
        @param a_pKey           Key to search for
        @param a_pDefault       Value to return if the key is not found
        @param a_pHasMultiple   Optionally receive notification of if there are
                                multiple entries for this key.

        @return a_pDefault      Key was not found in the section
        @return other           Value of the key
     */
    const SI_CHAR * GetValue(
            const SI_CHAR * a_pSection,
            const SI_CHAR * a_pKey,
            const SI_CHAR * a_pDefault = NULL,
            bool * a_pHasMultiple = NULL
            ) const;

    /** Retrieve a numeric value for a specific key. If multiple keys are enabled
        (see SetMultiKey) then only the first value associated with that key
        will be returned, see GetAllValues for getting all values with multikey.

        @param a_pSection       Section to search
        @param a_pKey           Key to search for
        @param a_nDefault       Value to return if the key is not found
        @param a_pHasMultiple   Optionally receive notification of if there are
                                multiple entries for this key.

        @return a_nDefault      Key was not found in the section
        @return other           Value of the key
     */
    long GetLongValue(
            const SI_CHAR * a_pSection,
            const SI_CHAR * a_pKey,
            long a_nDefault = 0,
            bool * a_pHasMultiple = NULL
            ) const;

    /** Retrieve a numeric value for a specific key. If multiple keys are enabled
        (see SetMultiKey) then only the first value associated with that key
        will be returned, see GetAllValues for getting all values with multikey.

        @param a_pSection       Section to search
        @param a_pKey           Key to search for
        @param a_nDefault       Value to return if the key is not found
        @param a_pHasMultiple   Optionally receive notification of if there are
                                multiple entries for this key.

        @return a_nDefault      Key was not found in the section
        @return other           Value of the key
     */
    double GetDoubleValue(
            const SI_CHAR * a_pSection,
            const SI_CHAR * a_pKey,
            double a_nDefault = 0,
            bool * a_pHasMultiple = NULL
            ) const;

    /** Retrieve a boolean value for a specific key. If multiple keys are enabled
        (see SetMultiKey) then only the first value associated with that key
        will be returned, see GetAllValues for getting all values with multikey.

        Strings starting with "t", "y", "on" or "1" are returned as logically true.
        Strings starting with "f", "n", "of" or "0" are returned as logically false.
        For all other values the default is returned. Character comparisons are
        case-insensitive.

        @param a_pSection       Section to search
        @param a_pKey           Key to search for
        @param a_bDefault       Value to return if the key is not found
        @param a_pHasMultiple   Optionally receive notification of if there are
                                multiple entries for this key.

        @return a_nDefault      Key was not found in the section
        @return other           Value of the key
     */
    bool GetBoolValue(
            const SI_CHAR * a_pSection,
            const SI_CHAR * a_pKey,
            bool a_bDefault = false,
            bool * a_pHasMultiple = NULL
            ) const;

    /** Add or update a section or value. This will always insert
        when multiple keys are enabled.

        @param a_pSection   Section to add or update
        @param a_pKey       Key to add or update. Set to NULL to
                            create an empty section.
        @param a_pValue     Value to set. Set to NULL to create an
                            empty section.
        @param a_pComment   Comment to be associated with the section or the
                            key. If a_pKey is NULL then it will be associated
                            with the section, otherwise the key. Note that a
                            comment may be set ONLY when the section or key is
                            first created (i.e. when this function returns the
                            value SI_INSERTED). If you wish to create a section
                            with a comment then you need to create the section
                            separately to the key. The comment string must be
                            in full comment form already (have a comment
                            character starting every line).
        @param a_bForceReplace  Should all existing values in a multi-key INI
                            file be replaced with this entry. This option has
                            no effect if not using multi-key files. The
                            difference between Delete/SetValue and SetValue
                            with a_bForceReplace = true, is that the load
                            order and comment will be preserved this way.

        @return SI_Error    See error definitions
        @return SI_UPDATED  Value was updated
        @return SI_INSERTED Value was inserted
     */
    SI_Error SetValue(
            const SI_CHAR * a_pSection,
            const SI_CHAR * a_pKey,
            const SI_CHAR * a_pValue,
            const SI_CHAR * a_pComment = NULL,
            bool a_bForceReplace = false
            ) {
        return AddEntry(a_pSection, a_pKey, a_pValue, a_pComment, a_bForceReplace, true);
    }

    /** Add or update a numeric value. This will always insert
        when multiple keys are enabled.

        @param a_pSection   Section to add or update
        @param a_pKey       Key to add or update.
        @param a_nValue     Value to set.
        @param a_pComment   Comment to be associated with the key. See the
                            notes on SetValue() for comments.
        @param a_bUseHex    By default the value will be written to the file
                            in decimal format. Set this to true to write it
                            as hexadecimal.
        @param a_bForceReplace  Should all existing values in a multi-key INI
                            file be replaced with this entry. This option has
                            no effect if not using multi-key files. The
                            difference between Delete/SetLongValue and
                            SetLongValue with a_bForceReplace = true, is that
                            the load order and comment will be preserved this
                            way.

        @return SI_Error    See error definitions
        @return SI_UPDATED  Value was updated
        @return SI_INSERTED Value was inserted
     */
    SI_Error SetLongValue(
            const SI_CHAR * a_pSection,
            const SI_CHAR * a_pKey,
            long a_nValue,
            const SI_CHAR * a_pComment = NULL,
            bool a_bUseHex = false,
            bool a_bForceReplace = false
            );

    /** Add or update a double value. This will always insert
        when multiple keys are enabled.

        @param a_pSection   Section to add or update
        @param a_pKey       Key to add or update.
        @param a_nValue     Value to set.
        @param a_pComment   Comment to be associated with the key. See the
                            notes on SetValue() for comments.
        @param a_bForceReplace  Should all existing values in a multi-key INI
                            file be replaced with this entry. This option has
                            no effect if not using multi-key files. The
                            difference between Delete/SetDoubleValue and
                            SetDoubleValue with a_bForceReplace = true, is that
                            the load order and comment will be preserved this
                            way.

        @return SI_Error    See error definitions
        @return SI_UPDATED  Value was updated
        @return SI_INSERTED Value was inserted
     */
    SI_Error SetDoubleValue(
            const SI_CHAR * a_pSection,
            const SI_CHAR * a_pKey,
            double a_nValue,
            const SI_CHAR * a_pComment = NULL,
            bool a_bForceReplace = false
            );

    /** Add or update a boolean value. This will always insert
        when multiple keys are enabled.

        @param a_pSection   Section to add or update
        @param a_pKey       Key to add or update.
        @param a_bValue     Value to set.
        @param a_pComment   Comment to be associated with the key. See the
                            notes on SetValue() for comments.
        @param a_bForceReplace  Should all existing values in a multi-key INI
                            file be replaced with this entry. This option has
                            no effect if not using multi-key files. The
                            difference between Delete/SetBoolValue and
                            SetBoolValue with a_bForceReplace = true, is that
                            the load order and comment will be preserved this
                            way.

        @return SI_Error    See error definitions
        @return SI_UPDATED  Value was updated
        @return SI_INSERTED Value was inserted
     */
    SI_Error SetBoolValue(
            const SI_CHAR * a_pSection,
            const SI_CHAR * a_pKey,
            bool a_bValue,
            const SI_CHAR * a_pComment = NULL,
            bool a_bForceReplace = false
            );

    /** Delete an entire section, or a key from a section. Note that the
        data returned by GetSection is invalid and must not be used after
        anything has been deleted from that section using this method.
        Note when multiple keys is enabled, this will delete all keys with
        that name; to selectively delete individual key/values, use
        DeleteValue.

        @param a_pSection       Section to delete key from, or if
                                a_pKey is NULL, the section to remove.
        @param a_pKey           Key to remove from the section. Set to
                                NULL to remove the entire section.
        @param a_bRemoveEmpty   If the section is empty after this key has
                                been deleted, should the empty section be
                                removed?

        @return true            Key or section was deleted.
        @return false           Key or section was not found.
     */
    bool Delete(
            const SI_CHAR * a_pSection,
            const SI_CHAR * a_pKey,
            bool a_bRemoveEmpty = false
            );

    /** Delete an entire section, or a key from a section. If value is
        provided, only remove keys with the value. Note that the data
        returned by GetSection is invalid and must not be used after
        anything has been deleted from that section using this method.
        Note when multiple keys is enabled, all keys with the value will
        be deleted.

        @param a_pSection       Section to delete key from, or if
                                a_pKey is NULL, the section to remove.
        @param a_pKey           Key to remove from the section. Set to
                                NULL to remove the entire section.
        @param a_pValue         Value of key to remove from the section.
                                Set to NULL to remove all keys.
        @param a_bRemoveEmpty   If the section is empty after this key has
                                been deleted, should the empty section be
                                removed?

        @return true            Key/value or section was deleted.
        @return false           Key/value or section was not found.
     */
    bool DeleteValue(
            const SI_CHAR * a_pSection,
            const SI_CHAR * a_pKey,
            const SI_CHAR * a_pValue,
            bool a_bRemoveEmpty = false
            );

    /*-----------------------------------------------------------------------*/
    /** Return a conversion object to convert text to the same encoding
        as is used by the Save(), SaveFile() and SaveString() functions.
        Use this to prepare the strings that you wish to append or prepend
        to the output INI data.
     */
    Converter GetConverter() const {
        return Converter(m_bStoreIsUtf8);
    }

    /*-----------------------------------------------------------------------*/

private:
    // copying is not permitted
    CSimpleIniTempl(const CSimpleIniTempl &); // disabled
    CSimpleIniTempl & operator=(const CSimpleIniTempl &); // disabled

    /** Parse the data looking for a file comment and store it if found.
     */
    SI_Error FindFileComment(
            SI_CHAR *& a_pData,
            bool a_bCopyStrings
            );

    /** Parse the data looking for the next valid entry. The memory pointed to
        by a_pData is modified by inserting NULL characters. The pointer is
        updated to the current location in the block of text.
     */
    bool FindEntry(
            SI_CHAR *& a_pData,
            const SI_CHAR *& a_pSection,
            const SI_CHAR *& a_pKey,
            const SI_CHAR *& a_pVal,
            const SI_CHAR *& a_pComment
            ) const;

    /** Add the section/key/value to our data.

        @param a_pSection   Section name. Sections will be created if they
                            don't already exist.
        @param a_pKey       Key name. May be NULL to create an empty section.
                            Existing entries will be updated. New entries will
                            be created.
        @param a_pValue     Value for the key.
        @param a_pComment   Comment to be associated with the section or the
                            key. If a_pKey is NULL then it will be associated
                            with the section, otherwise the key. This must be
                            a string in full comment form already (have a
                            comment character starting every line).
        @param a_bForceReplace  Should all existing values in a multi-key INI
                            file be replaced with this entry. This option has
                            no effect if not using multi-key files. The
                            difference between Delete/AddEntry and AddEntry
                            with a_bForceReplace = true, is that the load
                            order and comment will be preserved this way.
        @param a_bCopyStrings   Should copies of the strings be made or not.
                            If false then the pointers will be used as is.
     */
    SI_Error AddEntry(
            const SI_CHAR * a_pSection,
            const SI_CHAR * a_pKey,
            const SI_CHAR * a_pValue,
            const SI_CHAR * a_pComment,
            bool a_bForceReplace,
            bool a_bCopyStrings
            );

    /** Is the supplied character a whitespace character? */
    inline bool IsSpace(SI_CHAR ch) const {
        return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n');
    }

    /** Does the supplied character start a comment line? */
    inline bool IsComment(SI_CHAR ch) const {
        return (ch == ';' || ch == '#');
    }

    /** Skip over a newline character (or characters) for either DOS or UNIX */
    inline void SkipNewLine(SI_CHAR *& a_pData) const {
        a_pData += (*a_pData == '\r' && *(a_pData + 1) == '\n') ? 2 : 1;
    }

    /** Make a copy of the supplied string, replacing the original pointer */
    SI_Error CopyString(const SI_CHAR *& a_pString);

    /** Delete a string from the copied strings buffer if necessary */
    void DeleteString(const SI_CHAR * a_pString);

    /** Internal use of our string comparison function */
    bool IsLess(const SI_CHAR * a_pLeft, const SI_CHAR * a_pRight) const {
        const static SI_STRLESS isLess = SI_STRLESS();
        return isLess(a_pLeft, a_pRight);
    }

    bool IsMultiLineTag(const SI_CHAR * a_pData) const;
    bool IsMultiLineData(const SI_CHAR * a_pData) const;
    bool LoadMultiLineText(
            SI_CHAR *& a_pData,
            const SI_CHAR *& a_pVal,
            const SI_CHAR * a_pTagName,
            bool a_bAllowBlankLinesInComment = false
            ) const;
    bool IsNewLineChar(SI_CHAR a_c) const;

    bool OutputMultiLineText(
            OutputWriter & a_oOutput,
            Converter & a_oConverter,
            const SI_CHAR * a_pText
            ) const;

private:
    /** Copy of the INI file data in our character format. This will be
        modified when parsed to have NULL characters added after all
        interesting string entries. All of the string pointers to sections,
        keys and values point into this block of memory.
     */
    SI_CHAR * m_pData;

    /** Length of the data that we have stored. Used when deleting strings
        to determine if the string is stored here or in the allocated string
        buffer.
     */
    size_t m_uDataLen;

    /** File comment for this data, if one exists. */
    const SI_CHAR * m_pFileComment;

    /** Parsed INI data. Section -> (Key -> Value). */
    TSection m_data;

    /** This vector stores allocated memory for copies of strings that have
        been supplied after the file load. It will be empty unless SetValue()
        has been called.
     */
    TNamesDepend m_strings;

    /** Is the format of our datafile UTF-8 or MBCS? */
    bool m_bStoreIsUtf8;

    /** Are multiple values permitted for the same key? */
    bool m_bAllowMultiKey;

    /** Are data values permitted to span multiple lines? */
    bool m_bAllowMultiLine;

    /** Should spaces be written out surrounding the equals sign? */
    bool m_bSpaces;

    /** Next order value, used to ensure sections and keys are output in the
        same order that they are loaded/added.
     */
    int m_nOrder;
};

// ---------------------------------------------------------------------------
//                                  IMPLEMENTATION
// ---------------------------------------------------------------------------

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::CSimpleIniTempl(
        bool a_bIsUtf8,
        bool a_bAllowMultiKey,
        bool a_bAllowMultiLine
        )
: m_pData(0)
, m_uDataLen(0)
, m_pFileComment(NULL)
, m_bStoreIsUtf8(a_bIsUtf8)
, m_bAllowMultiKey(a_bAllowMultiKey)
, m_bAllowMultiLine(a_bAllowMultiLine)
, m_bSpaces(true)
, m_nOrder(0) {
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::~CSimpleIniTempl() {
    Reset();
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
void
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::Reset() {
    // remove all data
    delete[] m_pData;
    m_pData = NULL;
    m_uDataLen = 0;
    m_pFileComment = NULL;
    if (!m_data.empty()) {
        m_data.erase(m_data.begin(), m_data.end());
    }

    // remove all strings
    if (!m_strings.empty()) {
        typename TNamesDepend::iterator i = m_strings.begin();
        for (; i != m_strings.end(); ++i) {
            delete[] const_cast<SI_CHAR*> (i->pItem);
        }
        m_strings.erase(m_strings.begin(), m_strings.end());
    }
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::LoadFile(
        const char * a_pszFile
        ) {
    FILE * fp = NULL;
#if __STDC_WANT_SECURE_LIB__ && !_WIN32_WCE
    fopen_s(&fp, a_pszFile, "rb");
#else // !__STDC_WANT_SECURE_LIB__
    fp = fopen(a_pszFile, "rb");
#endif // __STDC_WANT_SECURE_LIB__
    if (!fp) {
        return SI_FILE;
    }
    SI_Error rc = LoadFile(fp);
    fclose(fp);
    return rc;
}

#ifdef SI_HAS_WIDE_FILE

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::LoadFile(
        const SI_WCHAR_T * a_pwszFile
        ) {
#ifdef _WIN32
    FILE * fp = NULL;
#if __STDC_WANT_SECURE_LIB__ && !_WIN32_WCE
    _wfopen_s(&fp, a_pwszFile, L"rb");
#else // !__STDC_WANT_SECURE_LIB__
    fp = _wfopen(a_pwszFile, L"rb");
#endif // __STDC_WANT_SECURE_LIB__
    if (!fp) return SI_FILE;
    SI_Error rc = LoadFile(fp);
    fclose(fp);
    return rc;
#else // !_WIN32 (therefore SI_CONVERT_ICU)
    char szFile[256];
    u_austrncpy(szFile, a_pwszFile, sizeof (szFile));
    return LoadFile(szFile);
#endif // _WIN32
}
#endif // SI_HAS_WIDE_FILE

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::LoadFile(
        FILE * a_fpFile
        ) {
    // load the raw file data
    int retval = fseek(a_fpFile, 0, SEEK_END);
    if (retval != 0) {
        return SI_FILE;
    }
    long lSize = ftell(a_fpFile);
    if (lSize < 0) {
        return SI_FILE;
    }
    if (lSize == 0) {
        return SI_OK;
    }

    // allocate and ensure NULL terminated
    char * pData = new(std::nothrow) char[lSize + 1];
    if (!pData) {
        return SI_NOMEM;
    }
    pData[lSize] = 0;

    // load data into buffer
    fseek(a_fpFile, 0, SEEK_SET);
    size_t uRead = fread(pData, sizeof (char), lSize, a_fpFile);
    if (uRead != (size_t) lSize) {
        delete[] pData;
        return SI_FILE;
    }

    // convert the raw data to unicode
    SI_Error rc = LoadData(pData, uRead);
    delete[] pData;
    return rc;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::LoadData(
        const char * a_pData,
        size_t a_uDataLen
        ) {
    SI_CONVERTER converter(m_bStoreIsUtf8);

    if (a_uDataLen == 0) {
        return SI_OK;
    }

    // consume the UTF-8 BOM if it exists
    if (m_bStoreIsUtf8 && a_uDataLen >= 3) {
        if (memcmp(a_pData, SI_UTF8_SIGNATURE, 3) == 0) {
            a_pData += 3;
            a_uDataLen -= 3;
        }
    }

    // determine the length of the converted data
    size_t uLen = converter.SizeFromStore(a_pData, a_uDataLen);
    if (uLen == (size_t) (-1)) {
        return SI_FAIL;
    }

    // allocate memory for the data, ensure that there is a NULL
    // terminator wherever the converted data ends
    SI_CHAR * pData = new(std::nothrow) SI_CHAR[uLen + 1];
    if (!pData) {
        return SI_NOMEM;
    }
    memset(pData, 0, sizeof (SI_CHAR)*(uLen + 1));

    // convert the data
    if (!converter.ConvertFromStore(a_pData, a_uDataLen, pData, uLen)) {
        delete[] pData;
        return SI_FAIL;
    }

    // parse it
    const static SI_CHAR empty = 0;
    SI_CHAR * pWork = pData;
    const SI_CHAR * pSection = &empty;
    const SI_CHAR * pItem = NULL;
    const SI_CHAR * pVal = NULL;
    const SI_CHAR * pComment = NULL;

    // We copy the strings if we are loading data into this class when we
    // already have stored some.
    bool bCopyStrings = (m_pData != NULL);

    // find a file comment if it exists, this is a comment that starts at the
    // beginning of the file and continues until the first blank line.
    SI_Error rc = FindFileComment(pWork, bCopyStrings);
    if (rc < 0) return rc;

    // add every entry in the file to the data table
    while (FindEntry(pWork, pSection, pItem, pVal, pComment)) {
        rc = AddEntry(pSection, pItem, pVal, pComment, false, bCopyStrings);
        if (rc < 0) return rc;
    }

    // store these strings if we didn't copy them
    if (bCopyStrings) {
        delete[] pData;
    } else {
        m_pData = pData;
        m_uDataLen = uLen + 1;
    }

    return SI_OK;
}

#ifdef SI_SUPPORT_IOSTREAMS

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::LoadData(
        std::istream & a_istream
        ) {
    std::string strData;
    char szBuf[512];
    do {
        a_istream.get(szBuf, sizeof (szBuf), '\0');
        strData.append(szBuf);
    } while (a_istream.good());
    return LoadData(strData);
}
#endif // SI_SUPPORT_IOSTREAMS

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::FindFileComment(
        SI_CHAR *& a_pData,
        bool a_bCopyStrings
        ) {
    // there can only be a single file comment
    if (m_pFileComment) {
        return SI_OK;
    }

    // Load the file comment as multi-line text, this will modify all of
    // the newline characters to be single \n chars
    if (!LoadMultiLineText(a_pData, m_pFileComment, NULL, false)) {
        return SI_OK;
    }

    // copy the string if necessary
    if (a_bCopyStrings) {
        SI_Error rc = CopyString(m_pFileComment);
        if (rc < 0) return rc;
    }

    return SI_OK;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
bool
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::FindEntry(
        SI_CHAR *& a_pData,
        const SI_CHAR *& a_pSection,
        const SI_CHAR *& a_pKey,
        const SI_CHAR *& a_pVal,
        const SI_CHAR *& a_pComment
        ) const {
    a_pComment = NULL;

    SI_CHAR * pTrail = NULL;
    while (*a_pData) {
        // skip spaces and empty lines
        while (*a_pData && IsSpace(*a_pData)) {
            ++a_pData;
        }
        if (!*a_pData) {
            break;
        }

        // skip processing of comment lines but keep a pointer to
        // the start of the comment.
        if (IsComment(*a_pData)) {
            LoadMultiLineText(a_pData, a_pComment, NULL, true);
            continue;
        }

        // process section names
        if (*a_pData == '[') {
            // skip leading spaces
            ++a_pData;
            while (*a_pData && IsSpace(*a_pData)) {
                ++a_pData;
            }

            // find the end of the section name (it may contain spaces)
            // and convert it to lowercase as necessary
            a_pSection = a_pData;
            while (*a_pData && *a_pData != ']' && !IsNewLineChar(*a_pData)) {
                ++a_pData;
            }

            // if it's an invalid line, just skip it
            if (*a_pData != ']') {
                continue;
            }

            // remove trailing spaces from the section
            pTrail = a_pData - 1;
            while (pTrail >= a_pSection && IsSpace(*pTrail)) {
                --pTrail;
            }
            ++pTrail;
            *pTrail = 0;

            // skip to the end of the line
            ++a_pData; // safe as checked that it == ']' above
            while (*a_pData && !IsNewLineChar(*a_pData)) {
                ++a_pData;
            }

            a_pKey = NULL;
            a_pVal = NULL;
            return true;
        }

        // find the end of the key name (it may contain spaces)
        // and convert it to lowercase as necessary
        a_pKey = a_pData;
        while (*a_pData && *a_pData != '=' && !IsNewLineChar(*a_pData)) {
            ++a_pData;
        }

        // if it's an invalid line, just skip it
        if (*a_pData != '=') {
            continue;
        }

        // empty keys are invalid
        if (a_pKey == a_pData) {
            while (*a_pData && !IsNewLineChar(*a_pData)) {
                ++a_pData;
            }
            continue;
        }

        // remove trailing spaces from the key
        pTrail = a_pData - 1;
        while (pTrail >= a_pKey && IsSpace(*pTrail)) {
            --pTrail;
        }
        ++pTrail;
        *pTrail = 0;

        // skip leading whitespace on the value
        ++a_pData; // safe as checked that it == '=' above
        while (*a_pData && !IsNewLineChar(*a_pData) && IsSpace(*a_pData)) {
            ++a_pData;
        }

        // find the end of the value which is the end of this line
        a_pVal = a_pData;
        while (*a_pData && !IsNewLineChar(*a_pData)) {
            ++a_pData;
        }

        // remove trailing spaces from the value
        pTrail = a_pData - 1;
        if (*a_pData) { // prepare for the next round
            SkipNewLine(a_pData);
        }
        while (pTrail >= a_pVal && IsSpace(*pTrail)) {
            --pTrail;
        }
        ++pTrail;
        *pTrail = 0;

        // check for multi-line entries
        if (m_bAllowMultiLine && IsMultiLineTag(a_pVal)) {
            // skip the "<<<" to get the tag that will end the multiline
            const SI_CHAR * pTagName = a_pVal + 3;
            return LoadMultiLineText(a_pData, a_pVal, pTagName);
        }

        // return the standard entry
        return true;
    }

    return false;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
bool
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::IsMultiLineTag(
        const SI_CHAR * a_pVal
        ) const {
    // check for the "<<<" prefix for a multi-line entry
    if (*a_pVal++ != '<') return false;
    if (*a_pVal++ != '<') return false;
    if (*a_pVal++ != '<') return false;
    return true;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
bool
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::IsMultiLineData(
        const SI_CHAR * a_pData
        ) const {
    // data is multi-line if it has any of the following features:
    //  * whitespace prefix
    //  * embedded newlines
    //  * whitespace suffix

    // empty string
    if (!*a_pData) {
        return false;
    }

    // check for prefix
    if (IsSpace(*a_pData)) {
        return true;
    }

    // embedded newlines
    while (*a_pData) {
        if (IsNewLineChar(*a_pData)) {
            return true;
        }
        ++a_pData;
    }

    // check for suffix
    if (IsSpace(*--a_pData)) {
        return true;
    }

    return false;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
bool
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::IsNewLineChar(
        SI_CHAR a_c
        ) const {
    return (a_c == '\n' || a_c == '\r');
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
bool
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::LoadMultiLineText(
        SI_CHAR *& a_pData,
        const SI_CHAR *& a_pVal,
        const SI_CHAR * a_pTagName,
        bool a_bAllowBlankLinesInComment
        ) const {
    // we modify this data to strip all newlines down to a single '\n'
    // character. This means that on Windows we need to strip out some
    // characters which will make the data shorter.
    // i.e.  LINE1-LINE1\r\nLINE2-LINE2\0 will become
    //       LINE1-LINE1\nLINE2-LINE2\0
    // The pDataLine entry is the pointer to the location in memory that
    // the current line needs to start to run following the existing one.
    // This may be the same as pCurrLine in which case no move is needed.
    SI_CHAR * pDataLine = a_pData;
    SI_CHAR * pCurrLine;

    // value starts at the current line
    a_pVal = a_pData;

    // find the end tag. This tag must start in column 1 and be
    // followed by a newline. No whitespace removal is done while
    // searching for this tag.
    SI_CHAR cEndOfLineChar = *a_pData;
    for (;;) {
        // if we are loading comments then we need a comment character as
        // the first character on every line
        if (!a_pTagName && !IsComment(*a_pData)) {
            // if we aren't allowing blank lines then we're done
            if (!a_bAllowBlankLinesInComment) {
                break;
            }

            // if we are allowing blank lines then we only include them
            // in this comment if another comment follows, so read ahead
            // to find out.
            SI_CHAR * pCurr = a_pData;
            int nNewLines = 0;
            while (IsSpace(*pCurr)) {
                if (IsNewLineChar(*pCurr)) {
                    ++nNewLines;
                    SkipNewLine(pCurr);
                } else {
                    ++pCurr;
                }
            }

            // we have a comment, add the blank lines to the output
            // and continue processing from here
            if (IsComment(*pCurr)) {
                for (; nNewLines > 0; --nNewLines) *pDataLine++ = '\n';
                a_pData = pCurr;
                continue;
            }

            // the comment ends here
            break;
        }

        // find the end of this line
        pCurrLine = a_pData;
        while (*a_pData && !IsNewLineChar(*a_pData)) ++a_pData;

        // move this line down to the location that it should be if necessary
        if (pDataLine < pCurrLine) {
            size_t nLen = (size_t) (a_pData - pCurrLine);
            memmove(pDataLine, pCurrLine, nLen * sizeof (SI_CHAR));
            pDataLine[nLen] = '\0';
        }

        // end the line with a NULL
        cEndOfLineChar = *a_pData;
        *a_pData = 0;

        // if are looking for a tag then do the check now. This is done before
        // checking for end of the data, so that if we have the tag at the end
        // of the data then the tag is removed correctly.
        if (a_pTagName &&
                (!IsLess(pDataLine, a_pTagName) && !IsLess(a_pTagName, pDataLine))) {
            break;
        }

        // if we are at the end of the data then we just automatically end
        // this entry and return the current data.
        if (!cEndOfLineChar) {
            return true;
        }

        // otherwise we need to process this newline to ensure that it consists
        // of just a single \n character.
        pDataLine += (a_pData - pCurrLine);
        *a_pData = cEndOfLineChar;
        SkipNewLine(a_pData);
        *pDataLine++ = '\n';
    }

    // if we didn't find a comment at all then return false
    if (a_pVal == a_pData) {
        a_pVal = NULL;
        return false;
    }

    // the data (which ends at the end of the last line) needs to be
    // null-terminated BEFORE before the newline character(s). If the
    // user wants a new line in the multi-line data then they need to
    // add an empty line before the tag.
    *--pDataLine = '\0';

    // if looking for a tag and if we aren't at the end of the data,
    // then move a_pData to the start of the next line.
    if (a_pTagName && cEndOfLineChar) {
        SI_ASSERT(IsNewLineChar(cEndOfLineChar));
        *a_pData = cEndOfLineChar;
        SkipNewLine(a_pData);
    }

    return true;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::CopyString(
        const SI_CHAR *& a_pString
        ) {
    size_t uLen = 0;
    if (sizeof (SI_CHAR) == sizeof (char)) {
        uLen = strlen((const char *) a_pString);
    } else if (sizeof (SI_CHAR) == sizeof (wchar_t)) {
        uLen = wcslen((const wchar_t *)a_pString);
    } else {
        for (; a_pString[uLen]; ++uLen) /*loop*/;
    }
    ++uLen; // NULL character
    SI_CHAR * pCopy = new(std::nothrow) SI_CHAR[uLen];
    if (!pCopy) {
        return SI_NOMEM;
    }
    memcpy(pCopy, a_pString, sizeof (SI_CHAR) * uLen);
    m_strings.push_back(pCopy);
    a_pString = pCopy;
    return SI_OK;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::AddEntry(
        const SI_CHAR * a_pSection,
        const SI_CHAR * a_pKey,
        const SI_CHAR * a_pValue,
        const SI_CHAR * a_pComment,
        bool a_bForceReplace,
        bool a_bCopyStrings
        ) {
    SI_Error rc;
    bool bInserted = false;

    SI_ASSERT(!a_pComment || IsComment(*a_pComment));

    // if we are copying strings then make a copy of the comment now
    // because we will need it when we add the entry.
    if (a_bCopyStrings && a_pComment) {
        rc = CopyString(a_pComment);
        if (rc < 0) return rc;
    }

    // create the section entry if necessary
    typename TSection::iterator iSection = m_data.find(a_pSection);
    if (iSection == m_data.end()) {
        // if the section doesn't exist then we need a copy as the
        // string needs to last beyond the end of this function
        if (a_bCopyStrings) {
            rc = CopyString(a_pSection);
            if (rc < 0) return rc;
        }

        // only set the comment if this is a section only entry
        Entry oSection(a_pSection, ++m_nOrder);
        if (a_pComment && (!a_pKey || !a_pValue)) {
            oSection.pComment = a_pComment;
        }

        typename TSection::value_type oEntry(oSection, TKeyVal());
        typedef typename TSection::iterator SectionIterator;
        std::pair<SectionIterator, bool> i = m_data.insert(oEntry);
        iSection = i.first;
        bInserted = true;
    }
    if (!a_pKey || !a_pValue) {
        // section only entries are specified with pItem and pVal as NULL
        return bInserted ? SI_INSERTED : SI_UPDATED;
    }

    // check for existence of the key
    TKeyVal & keyval = iSection->second;
    typename TKeyVal::iterator iKey = keyval.find(a_pKey);

    // remove all existing entries but save the load order and
    // comment of the first entry
    int nLoadOrder = ++m_nOrder;
    if (iKey != keyval.end() && m_bAllowMultiKey && a_bForceReplace) {
        const SI_CHAR * pComment = NULL;
        while (iKey != keyval.end() && !IsLess(a_pKey, iKey->first.pItem)) {
            if (iKey->first.nOrder < nLoadOrder) {
                nLoadOrder = iKey->first.nOrder;
                pComment = iKey->first.pComment;
            }
            ++iKey;
        }
        if (pComment) {
            DeleteString(a_pComment);
            a_pComment = pComment;
            CopyString(a_pComment);
        }
        Delete(a_pSection, a_pKey);
        iKey = keyval.end();
    }

    // make string copies if necessary
    bool bForceCreateNewKey = m_bAllowMultiKey && !a_bForceReplace;
    if (a_bCopyStrings) {
        if (bForceCreateNewKey || iKey == keyval.end()) {
            // if the key doesn't exist then we need a copy as the
            // string needs to last beyond the end of this function
            // because we will be inserting the key next
            rc = CopyString(a_pKey);
            if (rc < 0) return rc;
        }

        // we always need a copy of the value
        rc = CopyString(a_pValue);
        if (rc < 0) return rc;
    }

    // create the key entry
    if (iKey == keyval.end() || bForceCreateNewKey) {
        Entry oKey(a_pKey, nLoadOrder);
        if (a_pComment) {
            oKey.pComment = a_pComment;
        }
        typename TKeyVal::value_type oEntry(oKey, static_cast<const SI_CHAR *> (NULL));
        iKey = keyval.insert(oEntry);
        bInserted = true;
    }
    iKey->second = a_pValue;
    return bInserted ? SI_INSERTED : SI_UPDATED;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
const SI_CHAR *
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::GetValue(
        const SI_CHAR * a_pSection,
        const SI_CHAR * a_pKey,
        const SI_CHAR * a_pDefault,
        bool * a_pHasMultiple
        ) const {
    if (a_pHasMultiple) {
        *a_pHasMultiple = false;
    }
    if (!a_pSection || !a_pKey) {
        return a_pDefault;
    }
    typename TSection::const_iterator iSection = m_data.find(a_pSection);
    if (iSection == m_data.end()) {
        return a_pDefault;
    }
    typename TKeyVal::const_iterator iKeyVal = iSection->second.find(a_pKey);
    if (iKeyVal == iSection->second.end()) {
        return a_pDefault;
    }

    // check for multiple entries with the same key
    if (m_bAllowMultiKey && a_pHasMultiple) {
        typename TKeyVal::const_iterator iTemp = iKeyVal;
        if (++iTemp != iSection->second.end()) {
            if (!IsLess(a_pKey, iTemp->first.pItem)) {
                *a_pHasMultiple = true;
            }
        }
    }

    return iKeyVal->second;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
long
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::GetLongValue(
        const SI_CHAR * a_pSection,
        const SI_CHAR * a_pKey,
        long a_nDefault,
        bool * a_pHasMultiple
        ) const {
    // return the default if we don't have a value
    const SI_CHAR * pszValue = GetValue(a_pSection, a_pKey, NULL, a_pHasMultiple);
    if (!pszValue || !*pszValue) return a_nDefault;

    // convert to UTF-8/MBCS which for a numeric value will be the same as ASCII
    char szValue[64] = {0};
    SI_CONVERTER c(m_bStoreIsUtf8);
    if (!c.ConvertToStore(pszValue, szValue, sizeof (szValue))) {
        return a_nDefault;
    }

    // handle the value as hex if prefaced with "0x"
    long nValue = a_nDefault;
    char * pszSuffix = szValue;
    if (szValue[0] == '0' && (szValue[1] == 'x' || szValue[1] == 'X')) {
        if (!szValue[2]) return a_nDefault;
        nValue = strtol(&szValue[2], &pszSuffix, 16);
    } else {
        nValue = strtol(szValue, &pszSuffix, 10);
    }

    // any invalid strings will return the default value
    if (*pszSuffix) {
        return a_nDefault;
    }

    return nValue;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::SetLongValue(
        const SI_CHAR * a_pSection,
        const SI_CHAR * a_pKey,
        long a_nValue,
        const SI_CHAR * a_pComment,
        bool a_bUseHex,
        bool a_bForceReplace
        ) {
    // use SetValue to create sections
    if (!a_pSection || !a_pKey) return SI_FAIL;

    // convert to an ASCII string
    char szInput[64];
#if __STDC_WANT_SECURE_LIB__ && !_WIN32_WCE
    sprintf_s(szInput, a_bUseHex ? "0x%lx" : "%ld", a_nValue);
#else // !__STDC_WANT_SECURE_LIB__
    sprintf(szInput, a_bUseHex ? "0x%lx" : "%ld", a_nValue);
#endif // __STDC_WANT_SECURE_LIB__

    // convert to output text
    SI_CHAR szOutput[64];
    SI_CONVERTER c(m_bStoreIsUtf8);
    c.ConvertFromStore(szInput, strlen(szInput) + 1,
            szOutput, sizeof (szOutput) / sizeof (SI_CHAR));

    // actually add it
    return AddEntry(a_pSection, a_pKey, szOutput, a_pComment, a_bForceReplace, true);
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
double
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::GetDoubleValue(
        const SI_CHAR * a_pSection,
        const SI_CHAR * a_pKey,
        double a_nDefault,
        bool * a_pHasMultiple
        ) const {
    // return the default if we don't have a value
    const SI_CHAR * pszValue = GetValue(a_pSection, a_pKey, NULL, a_pHasMultiple);
    if (!pszValue || !*pszValue) return a_nDefault;

    // convert to UTF-8/MBCS which for a numeric value will be the same as ASCII
    char szValue[64] = {0};
    SI_CONVERTER c(m_bStoreIsUtf8);
    if (!c.ConvertToStore(pszValue, szValue, sizeof (szValue))) {
        return a_nDefault;
    }

    char * pszSuffix = NULL;
    double nValue = strtod(szValue, &pszSuffix);

    // any invalid strings will return the default value
    if (!pszSuffix || *pszSuffix) {
        return a_nDefault;
    }

    return nValue;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::SetDoubleValue(
        const SI_CHAR * a_pSection,
        const SI_CHAR * a_pKey,
        double a_nValue,
        const SI_CHAR * a_pComment,
        bool a_bForceReplace
        ) {
    // use SetValue to create sections
    if (!a_pSection || !a_pKey) return SI_FAIL;

    // convert to an ASCII string
    char szInput[64];
#if __STDC_WANT_SECURE_LIB__ && !_WIN32_WCE
    sprintf_s(szInput, "%f", a_nValue);
#else // !__STDC_WANT_SECURE_LIB__
    sprintf(szInput, "%f", a_nValue);
#endif // __STDC_WANT_SECURE_LIB__

    // convert to output text
    SI_CHAR szOutput[64];
    SI_CONVERTER c(m_bStoreIsUtf8);
    c.ConvertFromStore(szInput, strlen(szInput) + 1,
            szOutput, sizeof (szOutput) / sizeof (SI_CHAR));

    // actually add it
    return AddEntry(a_pSection, a_pKey, szOutput, a_pComment, a_bForceReplace, true);
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
bool
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::GetBoolValue(
        const SI_CHAR * a_pSection,
        const SI_CHAR * a_pKey,
        bool a_bDefault,
        bool * a_pHasMultiple
        ) const {
    // return the default if we don't have a value
    const SI_CHAR * pszValue = GetValue(a_pSection, a_pKey, NULL, a_pHasMultiple);
    if (!pszValue || !*pszValue) return a_bDefault;

    // we only look at the minimum number of characters
    switch (pszValue[0]) {
        case 't': case 'T': // true
        case 'y': case 'Y': // yes
        case '1': // 1 (one)
            return true;

        case 'f': case 'F': // false
        case 'n': case 'N': // no
        case '0': // 0 (zero)
            return false;

        case 'o': case 'O':
            if (pszValue[1] == 'n' || pszValue[1] == 'N') return true; // on
            if (pszValue[1] == 'f' || pszValue[1] == 'F') return false; // off
            break;
    }

    // no recognized value, return the default
    return a_bDefault;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::SetBoolValue(
        const SI_CHAR * a_pSection,
        const SI_CHAR * a_pKey,
        bool a_bValue,
        const SI_CHAR * a_pComment,
        bool a_bForceReplace
        ) {
    // use SetValue to create sections
    if (!a_pSection || !a_pKey) return SI_FAIL;

    // convert to an ASCII string
    const char * pszInput = a_bValue ? "true" : "false";

    // convert to output text
    SI_CHAR szOutput[64];
    SI_CONVERTER c(m_bStoreIsUtf8);
    c.ConvertFromStore(pszInput, strlen(pszInput) + 1,
            szOutput, sizeof (szOutput) / sizeof (SI_CHAR));

    // actually add it
    return AddEntry(a_pSection, a_pKey, szOutput, a_pComment, a_bForceReplace, true);
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
bool
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::GetAllValues(
        const SI_CHAR * a_pSection,
        const SI_CHAR * a_pKey,
        TNamesDepend & a_values
        ) const {
    a_values.clear();

    if (!a_pSection || !a_pKey) {
        return false;
    }
    typename TSection::const_iterator iSection = m_data.find(a_pSection);
    if (iSection == m_data.end()) {
        return false;
    }
    typename TKeyVal::const_iterator iKeyVal = iSection->second.find(a_pKey);
    if (iKeyVal == iSection->second.end()) {
        return false;
    }

    // insert all values for this key
    a_values.push_back(Entry(iKeyVal->second, iKeyVal->first.pComment, iKeyVal->first.nOrder));
    if (m_bAllowMultiKey) {
        ++iKeyVal;
        while (iKeyVal != iSection->second.end() && !IsLess(a_pKey, iKeyVal->first.pItem)) {
            a_values.push_back(Entry(iKeyVal->second, iKeyVal->first.pComment, iKeyVal->first.nOrder));
            ++iKeyVal;
        }
    }

    return true;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
int
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::GetSectionSize(
        const SI_CHAR * a_pSection
        ) const {
    if (!a_pSection) {
        return -1;
    }

    typename TSection::const_iterator iSection = m_data.find(a_pSection);
    if (iSection == m_data.end()) {
        return -1;
    }
    const TKeyVal & section = iSection->second;

    // if multi-key isn't permitted then the section size is
    // the number of keys that we have.
    if (!m_bAllowMultiKey || section.empty()) {
        return (int) section.size();
    }

    // otherwise we need to count them
    int nCount = 0;
    const SI_CHAR * pLastKey = NULL;
    typename TKeyVal::const_iterator iKeyVal = section.begin();
    for (int n = 0; iKeyVal != section.end(); ++iKeyVal, ++n) {
        if (!pLastKey || IsLess(pLastKey, iKeyVal->first.pItem)) {
            ++nCount;
            pLastKey = iKeyVal->first.pItem;
        }
    }
    return nCount;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
const typename CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::TKeyVal *
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::GetSection(
        const SI_CHAR * a_pSection
        ) const {
    if (a_pSection) {
        typename TSection::const_iterator i = m_data.find(a_pSection);
        if (i != m_data.end()) {
            return &(i->second);
        }
    }
    return 0;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
void
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::GetAllSections(
        TNamesDepend & a_names
        ) const {
    a_names.clear();
    typename TSection::const_iterator i = m_data.begin();
    for (int n = 0; i != m_data.end(); ++i, ++n) {
        a_names.push_back(i->first);
    }
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
bool
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::GetAllKeys(
        const SI_CHAR * a_pSection,
        TNamesDepend & a_names
        ) const {
    a_names.clear();

    if (!a_pSection) {
        return false;
    }

    typename TSection::const_iterator iSection = m_data.find(a_pSection);
    if (iSection == m_data.end()) {
        return false;
    }

    const TKeyVal & section = iSection->second;
    const SI_CHAR * pLastKey = NULL;
    typename TKeyVal::const_iterator iKeyVal = section.begin();
    for (int n = 0; iKeyVal != section.end(); ++iKeyVal, ++n) {
        if (!pLastKey || IsLess(pLastKey, iKeyVal->first.pItem)) {
            a_names.push_back(iKeyVal->first);
            pLastKey = iKeyVal->first.pItem;
        }
    }

    return true;
}

#if 0
template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::SaveFile(
        const char * a_pszFile,
        bool a_bAddSignature
        ) const {
    FILE * fp = NULL;
#if __STDC_WANT_SECURE_LIB__ && !_WIN32_WCE
    fopen_s(&fp, a_pszFile, "wb");
#else // !__STDC_WANT_SECURE_LIB__
    fp = fopen(a_pszFile, "wb");
#endif // __STDC_WANT_SECURE_LIB__
    if (!fp) return SI_FILE;
    SI_Error rc = SaveFile(fp, a_bAddSignature);
    fclose(fp);
    return rc;
}

#ifdef SI_HAS_WIDE_FILE

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::SaveFile(
        const SI_WCHAR_T * a_pwszFile,
        bool a_bAddSignature
        ) const {
#ifdef _WIN32
    FILE * fp = NULL;
#if __STDC_WANT_SECURE_LIB__ && !_WIN32_WCE
    _wfopen_s(&fp, a_pwszFile, L"wb");
#else // !__STDC_WANT_SECURE_LIB__
    fp = _wfopen(a_pwszFile, L"wb");
#endif // __STDC_WANT_SECURE_LIB__
    if (!fp) return SI_FILE;
    SI_Error rc = SaveFile(fp, a_bAddSignature);
    fclose(fp);
    return rc;
#else // !_WIN32 (therefore SI_CONVERT_ICU)
    char szFile[256];
    u_austrncpy(szFile, a_pwszFile, sizeof (szFile));
    return SaveFile(szFile, a_bAddSignature);
#endif // _WIN32
}
#endif // SI_HAS_WIDE_FILE

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::SaveFile(
        FILE * a_pFile,
        bool a_bAddSignature
        ) const {
    FileWriter writer(a_pFile);
    return Save(writer, a_bAddSignature);
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
SI_Error
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::Save(
        OutputWriter & a_oOutput,
        bool a_bAddSignature
        ) const {
    Converter convert(m_bStoreIsUtf8);

    // add the UTF-8 signature if it is desired
    if (m_bStoreIsUtf8 && a_bAddSignature) {
        a_oOutput.Write(SI_UTF8_SIGNATURE);
    }

    // get all of the sections sorted in load order
    TNamesDepend oSections;
    GetAllSections(oSections);
#if defined(_MSC_VER) && _MSC_VER <= 1200
    oSections.sort();
#elif defined(__BORLANDC__)
    oSections.sort(Entry::LoadOrder());
#else
    oSections.sort(typename Entry::LoadOrder());
#endif

    // write the file comment if we have one
    bool bNeedNewLine = false;
    if (m_pFileComment) {
        if (!OutputMultiLineText(a_oOutput, convert, m_pFileComment)) {
            return SI_FAIL;
        }
        bNeedNewLine = true;
    }

    // iterate through our sections and output the data
    typename TNamesDepend::const_iterator iSection = oSections.begin();
    for (; iSection != oSections.end(); ++iSection) {
        // write out the comment if there is one
        if (iSection->pComment) {
            if (bNeedNewLine) {
                a_oOutput.Write(SI_NEWLINE_A);
                a_oOutput.Write(SI_NEWLINE_A);
            }
            if (!OutputMultiLineText(a_oOutput, convert, iSection->pComment)) {
                return SI_FAIL;
            }
            bNeedNewLine = false;
        }

        if (bNeedNewLine) {
            a_oOutput.Write(SI_NEWLINE_A);
            a_oOutput.Write(SI_NEWLINE_A);
            bNeedNewLine = false;
        }

        // write the section (unless there is no section name)
        if (*iSection->pItem) {
            if (!convert.ConvertToStore(iSection->pItem)) {
                return SI_FAIL;
            }
            a_oOutput.Write("[");
            a_oOutput.Write(convert.Data());
            a_oOutput.Write("]");
            a_oOutput.Write(SI_NEWLINE_A);
        }

        // get all of the keys sorted in load order
        TNamesDepend oKeys;
        GetAllKeys(iSection->pItem, oKeys);
#if defined(_MSC_VER) && _MSC_VER <= 1200
        oKeys.sort();
#elif defined(__BORLANDC__)
        oKeys.sort(Entry::LoadOrder());
#else
        oKeys.sort(typename Entry::LoadOrder());
#endif

        // write all keys and values
        typename TNamesDepend::const_iterator iKey = oKeys.begin();
        for (; iKey != oKeys.end(); ++iKey) {
            // get all values for this key
            TNamesDepend oValues;
            GetAllValues(iSection->pItem, iKey->pItem, oValues);

            typename TNamesDepend::const_iterator iValue = oValues.begin();
            for (; iValue != oValues.end(); ++iValue) {
                // write out the comment if there is one
                if (iValue->pComment) {
                    a_oOutput.Write(SI_NEWLINE_A);
                    if (!OutputMultiLineText(a_oOutput, convert, iValue->pComment)) {
                        return SI_FAIL;
                    }
                }

                // write the key
                if (!convert.ConvertToStore(iKey->pItem)) {
                    return SI_FAIL;
                }
                a_oOutput.Write(convert.Data());

                // write the value
                if (!convert.ConvertToStore(iValue->pItem)) {
                    return SI_FAIL;
                }
                a_oOutput.Write(m_bSpaces ? " = " : "=");
                if (m_bAllowMultiLine && IsMultiLineData(iValue->pItem)) {
                    // multi-line data needs to be processed specially to ensure
                    // that we use the correct newline format for the current system
                    a_oOutput.Write("<<<END_OF_TEXT" SI_NEWLINE_A);
                    if (!OutputMultiLineText(a_oOutput, convert, iValue->pItem)) {
                        return SI_FAIL;
                    }
                    a_oOutput.Write("END_OF_TEXT");
                } else {
                    a_oOutput.Write(convert.Data());
                }
                a_oOutput.Write(SI_NEWLINE_A);
            }
        }

        bNeedNewLine = true;
    }

    return SI_OK;
}

#endif

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
bool
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::OutputMultiLineText(
        OutputWriter & a_oOutput,
        Converter & a_oConverter,
        const SI_CHAR * a_pText
        ) const {
    const SI_CHAR * pEndOfLine;
    SI_CHAR cEndOfLineChar = *a_pText;
    while (cEndOfLineChar) {
        // find the end of this line
        pEndOfLine = a_pText;
        for (; *pEndOfLine && *pEndOfLine != '\n'; ++pEndOfLine) /*loop*/;
        cEndOfLineChar = *pEndOfLine;

        // temporarily null terminate, convert and output the line
        *const_cast<SI_CHAR*> (pEndOfLine) = 0;
        if (!a_oConverter.ConvertToStore(a_pText)) {
            return false;
        }
        *const_cast<SI_CHAR*> (pEndOfLine) = cEndOfLineChar;
        a_pText += (pEndOfLine - a_pText) + 1;
        a_oOutput.Write(a_oConverter.Data());
        a_oOutput.Write(SI_NEWLINE_A);
    }
    return true;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
bool
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::Delete(
        const SI_CHAR * a_pSection,
        const SI_CHAR * a_pKey,
        bool a_bRemoveEmpty
        ) {
    return DeleteValue(a_pSection, a_pKey, NULL, a_bRemoveEmpty);
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
bool
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::DeleteValue(
        const SI_CHAR * a_pSection,
        const SI_CHAR * a_pKey,
        const SI_CHAR * a_pValue,
        bool a_bRemoveEmpty
        ) {
    if (!a_pSection) {
        return false;
    }

    typename TSection::iterator iSection = m_data.find(a_pSection);
    if (iSection == m_data.end()) {
        return false;
    }

    // remove a single key if we have a keyname
    if (a_pKey) {
        typename TKeyVal::iterator iKeyVal = iSection->second.find(a_pKey);
        if (iKeyVal == iSection->second.end()) {
            return false;
        }

        const static SI_STRLESS isLess = SI_STRLESS();

        // remove any copied strings and then the key
        typename TKeyVal::iterator iDelete;
        bool bDeleted = false;
        do {
            iDelete = iKeyVal++;

            if (a_pValue == NULL ||
                    (isLess(a_pValue, iDelete->second) == false &&
                    isLess(iDelete->second, a_pValue) == false)) {
                DeleteString(iDelete->first.pItem);
                DeleteString(iDelete->second);
                iSection->second.erase(iDelete);
                bDeleted = true;
            }
        } while (iKeyVal != iSection->second.end()
                && !IsLess(a_pKey, iKeyVal->first.pItem));

        if (!bDeleted) {
            return false;
        }

        // done now if the section is not empty or we are not pruning away
        // the empty sections. Otherwise let it fall through into the section
        // deletion code
        if (!a_bRemoveEmpty || !iSection->second.empty()) {
            return true;
        }
    } else {
        // delete all copied strings from this section. The actual
        // entries will be removed when the section is removed.
        typename TKeyVal::iterator iKeyVal = iSection->second.begin();
        for (; iKeyVal != iSection->second.end(); ++iKeyVal) {
            DeleteString(iKeyVal->first.pItem);
            DeleteString(iKeyVal->second);
        }
    }

    // delete the section itself
    DeleteString(iSection->first.pItem);
    m_data.erase(iSection);

    return true;
}

template<class SI_CHAR, class SI_STRLESS, class SI_CONVERTER>
void
CSimpleIniTempl<SI_CHAR, SI_STRLESS, SI_CONVERTER>::DeleteString(
        const SI_CHAR * a_pString
        ) {
    // strings may exist either inside the data block, or they will be
    // individually allocated and stored in m_strings. We only physically
    // delete those stored in m_strings.
    if (a_pString < m_pData || a_pString >= m_pData + m_uDataLen) {
        typename TNamesDepend::iterator i = m_strings.begin();
        for (; i != m_strings.end(); ++i) {
            if (a_pString == i->pItem) {
                delete[] const_cast<SI_CHAR*> (i->pItem);
                m_strings.erase(i);
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
//                              CONVERSION FUNCTIONS
// ---------------------------------------------------------------------------

// Defines the conversion classes for different libraries. Before including
// SimpleIni.h, set the converter that you wish you use by defining one of the
// following symbols.
//
//  SI_CONVERT_GENERIC      Use the Unicode reference conversion library in
//                          the accompanying files ConvertUTF.h/c
//  SI_CONVERT_ICU          Use the IBM ICU conversion library. Requires
//                          ICU headers on include path and icuuc.lib
//  SI_CONVERT_WIN32        Use the Win32 API functions for conversion.

#if !defined(SI_CONVERT_GENERIC) && !defined(SI_CONVERT_WIN32) && !defined(SI_CONVERT_ICU)
#ifdef _WIN32
#define SI_CONVERT_WIN32
#else
#define SI_CONVERT_GENERIC
#endif
#endif

/**
 * Generic case-sensitive less than comparison. This class returns numerically
 * ordered ASCII case-sensitive text for all possible sizes and types of
 * SI_CHAR.
 */
template<class SI_CHAR>
struct SI_GenericCase {

    bool operator()(const SI_CHAR * pLeft, const SI_CHAR * pRight) const {
        long cmp;
        for (; *pLeft && *pRight; ++pLeft, ++pRight) {
            cmp = (long) *pLeft - (long) *pRight;
            if (cmp != 0) {
                return cmp < 0;
            }
        }
        return *pRight != 0;
    }
};

/**
 * Generic ASCII case-insensitive less than comparison. This class returns
 * numerically ordered ASCII case-insensitive text for all possible sizes
 * and types of SI_CHAR. It is not safe for MBCS text comparison where
 * ASCII A-Z characters are used in the encoding of multi-byte characters.
 */
template<class SI_CHAR>
struct SI_GenericNoCase {

    inline SI_CHAR locase(SI_CHAR ch) const {
        return (ch < 'A' || ch > 'Z') ? ch : (ch - 'A' + 'a');
    }

    bool operator()(const SI_CHAR * pLeft, const SI_CHAR * pRight) const {
        long cmp;
        for (; *pLeft && *pRight; ++pLeft, ++pRight) {
            cmp = (long) locase(*pLeft) - (long) locase(*pRight);
            if (cmp != 0) {
                return cmp < 0;
            }
        }
        return *pRight != 0;
    }
};

/**
 * Null conversion class for MBCS/UTF-8 to char (or equivalent).
 */
template<class SI_CHAR>
class SI_ConvertA {
    bool m_bStoreIsUtf8;
protected:

    SI_ConvertA() {
    }
public:

    SI_ConvertA(bool a_bStoreIsUtf8) : m_bStoreIsUtf8(a_bStoreIsUtf8) {
    }

    /* copy and assignment */
    SI_ConvertA(const SI_ConvertA & rhs) {
        operator=(rhs);
    }

    SI_ConvertA & operator=(const SI_ConvertA & rhs) {
        m_bStoreIsUtf8 = rhs.m_bStoreIsUtf8;
        return *this;
    }

    /** Calculate the number of SI_CHAR required for converting the input
     * from the storage format. The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  Data in storage format to be converted to SI_CHAR.
     * @param a_uInputDataLen Length of storage format data in bytes. This
     *                      must be the actual length of the data, including
     *                      NULL byte if NULL terminated string is required.
     * @return              Number of SI_CHAR required by the string when
     *                      converted. If there are embedded NULL bytes in the
     *                      input data, only the string up and not including
     *                      the NULL byte will be converted.
     * @return              -1 cast to size_t on a conversion error.
     */
    size_t SizeFromStore(
            const char * a_pInputData,
            size_t a_uInputDataLen) {
        (void) a_pInputData;
        SI_ASSERT(a_uInputDataLen != (size_t) - 1);

        // ASCII/MBCS/UTF-8 needs no conversion
        return a_uInputDataLen;
    }

    /** Convert the input string from the storage format to SI_CHAR.
     * The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  Data in storage format to be converted to SI_CHAR.
     * @param a_uInputDataLen Length of storage format data in bytes. This
     *                      must be the actual length of the data, including
     *                      NULL byte if NULL terminated string is required.
     * @param a_pOutputData Pointer to the output buffer to received the
     *                      converted data.
     * @param a_uOutputDataSize Size of the output buffer in SI_CHAR.
     * @return              true if all of the input data was successfully
     *                      converted.
     */
    bool ConvertFromStore(
            const char * a_pInputData,
            size_t a_uInputDataLen,
            SI_CHAR * a_pOutputData,
            size_t a_uOutputDataSize) {
        // ASCII/MBCS/UTF-8 needs no conversion
        if (a_uInputDataLen > a_uOutputDataSize) {
            return false;
        }
        memcpy(a_pOutputData, a_pInputData, a_uInputDataLen);
        return true;
    }

    /** Calculate the number of char required by the storage format of this
     * data. The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  NULL terminated string to calculate the number of
     *                      bytes required to be converted to storage format.
     * @return              Number of bytes required by the string when
     *                      converted to storage format. This size always
     *                      includes space for the terminating NULL character.
     * @return              -1 cast to size_t on a conversion error.
     */
    size_t SizeToStore(
            const SI_CHAR * a_pInputData) {
        // ASCII/MBCS/UTF-8 needs no conversion
        return strlen((const char *) a_pInputData) + 1;
    }

    /** Convert the input string to the storage format of this data.
     * The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  NULL terminated source string to convert. All of
     *                      the data will be converted including the
     *                      terminating NULL character.
     * @param a_pOutputData Pointer to the buffer to receive the converted
     *                      string.
     * @param a_uOutputDataSize Size of the output buffer in char.
     * @return              true if all of the input data, including the
     *                      terminating NULL character was successfully
     *                      converted.
     */
    bool ConvertToStore(
            const SI_CHAR * a_pInputData,
            char * a_pOutputData,
            size_t a_uOutputDataSize) {
        // calc input string length (SI_CHAR type and size independent)
        size_t uInputLen = strlen((const char *) a_pInputData) + 1;
        if (uInputLen > a_uOutputDataSize) {
            return false;
        }

        // ascii/UTF-8 needs no conversion
        memcpy(a_pOutputData, a_pInputData, uInputLen);
        return true;
    }
};


// ---------------------------------------------------------------------------
//                              SI_CONVERT_GENERIC
// ---------------------------------------------------------------------------
#ifdef SI_CONVERT_GENERIC

#define SI_Case     SI_GenericCase
#define SI_NoCase   SI_GenericNoCase

#include <wchar.h>
//#include "ConvertUTF.hpp"
/*
 * Copyright 2001-2004 Unicode, Inc.
 *
 * Disclaimer
 *
 * This source code is provided as is by Unicode, Inc. No claims are
 * made as to fitness for any particular purpose. No warranties of any
 * kind are expressed or implied. The recipient agrees to determine
 * applicability of information provided. If this file has been
 * purchased on magnetic or optical media from Unicode, Inc., the
 * sole remedy for any claim will be exchange of defective media
 * within 90 days of receipt.
 *
 * Limitations on Rights to Redistribute This Code
 *
 * Unicode, Inc. hereby grants the right to freely use the information
 * supplied in this file in the creation of products supporting the
 * Unicode Standard, and to make copies of this file in any form
 * for internal or external distribution as long as this notice
 * remains attached.
 */

/* ---------------------------------------------------------------------

    Conversions between UTF32, UTF-16, and UTF-8.  Header file.

    Several funtions are included here, forming a complete set of
    conversions between the three formats.  UTF-7 is not included
    here, but is handled in a separate source file.

    Each of these routines takes pointers to input buffers and output
    buffers.  The input buffers are const.

    Each routine converts the text between *sourceStart and sourceEnd,
    putting the result into the buffer between *targetStart and
    targetEnd. Note: the end pointers are *after* the last item: e.g.
 *(sourceEnd - 1) is the last item.

    The return result indicates whether the conversion was successful,
    and if not, whether the problem was in the source or target buffers.
    (Only the first encountered problem is indicated.)

    After the conversion, *sourceStart and *targetStart are both
    updated to point to the end of last text successfully converted in
    the respective buffers.

    Input parameters:
        sourceStart - pointer to a pointer to the source buffer.
                The contents of this are modified on return so that
                it points at the next thing to be converted.
        targetStart - similarly, pointer to pointer to the target buffer.
        sourceEnd, targetEnd - respectively pointers to the ends of the
                two buffers, for overflow checking only.

    These conversion functions take a ConversionFlags argument. When this
    flag is set to strict, both irregular sequences and isolated surrogates
    will cause an error.  When the flag is set to lenient, both irregular
    sequences and isolated surrogates are converted.

    Whether the flag is strict or lenient, all illegal sequences will cause
    an error return. This includes sequences such as: <F4 90 80 80>, <C0 80>,
    or <A0> in UTF-8, and values above 0x10FFFF in UTF-32. Conformant code
    must check for illegal sequences.

    When the flag is set to lenient, characters over 0x10FFFF are converted
    to the replacement character; otherwise (when the flag is set to strict)
    they constitute an error.

    Output parameters:
        The value "sourceIllegal" is returned from some routines if the input
        sequence is malformed.  When "sourceIllegal" is returned, the source
        value will point to the illegal value that caused the problem. E.g.,
        in UTF-8 when a sequence is malformed, it points to the start of the
        malformed sequence.

    Author: Mark E. Davis, 1994.
    Rev History: Rick McGowan, fixes & updates May 2001.
                 Fixes & updates, Sept 2001.

------------------------------------------------------------------------ */

/* ---------------------------------------------------------------------
    The following 4 definitions are compiler-specific.
    The C standard does not guarantee that wchar_t has at least
    16 bits, so wchar_t is no less portable than unsigned short!
    All should be unsigned values to avoid sign extension during
    bit mask & shift operations.
------------------------------------------------------------------------ */

typedef unsigned int UTF32; /* at least 32 bits */
typedef unsigned short UTF16; /* at least 16 bits */
typedef unsigned char UTF8; /* typically 8 bits */

/* Some fundamental constants */
#define UNI_REPLACEMENT_CHAR (UTF32)0x0000FFFD
#define UNI_MAX_BMP (UTF32)0x0000FFFF
#define UNI_MAX_UTF16 (UTF32)0x0010FFFF
#define UNI_MAX_UTF32 (UTF32)0x7FFFFFFF
#define UNI_MAX_LEGAL_UTF32 (UTF32)0x0010FFFF

typedef enum {
    conversionOK, /* conversion successful */
    sourceExhausted, /* partial character in source, but hit end */
    targetExhausted, /* insuff. room in target for conversion */
    sourceIllegal /* source sequence is illegal/malformed */
} ConversionResult;

typedef enum {
    strictConversion = 0,
    lenientConversion
} ConversionFlags;

/* This is for C++ and does no harm in C */
#ifdef __cplusplus
extern "C" {
#endif
#if 0
    ConversionResult ConvertUTF8toUTF16(
            const UTF8** sourceStart, const UTF8* sourceEnd,
            UTF16** targetStart, UTF16* targetEnd, ConversionFlags flags);

    ConversionResult ConvertUTF16toUTF8(
            const UTF16** sourceStart, const UTF16* sourceEnd,
            UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags);

    ConversionResult ConvertUTF8toUTF32(
            const UTF8** sourceStart, const UTF8* sourceEnd,
            UTF32** targetStart, UTF32* targetEnd, ConversionFlags flags);

    ConversionResult ConvertUTF32toUTF8(
            const UTF32** sourceStart, const UTF32* sourceEnd,
            UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags);

    ConversionResult ConvertUTF16toUTF32(
            const UTF16** sourceStart, const UTF16* sourceEnd,
            UTF32** targetStart, UTF32* targetEnd, ConversionFlags flags);

    ConversionResult ConvertUTF32toUTF16(
            const UTF32** sourceStart, const UTF32* sourceEnd,
            UTF16** targetStart, UTF16* targetEnd, ConversionFlags flags);

    Boolean isLegalUTF8Sequence(const UTF8 *source, const UTF8 *sourceEnd);
#endif


#ifdef CVTUTF_DEBUG
#include <stdio.h>
#endif

    static const int halfShift = 10; /* used for shifting by 10 bits */

    static const UTF32 halfBase = 0x0010000UL;
    static const UTF32 halfMask = 0x3FFUL;

#define UNI_SUR_HIGH_START  (UTF32)0xD800
#define UNI_SUR_HIGH_END    (UTF32)0xDBFF
#define UNI_SUR_LOW_START   (UTF32)0xDC00
#define UNI_SUR_LOW_END     (UTF32)0xDFFF

    /* --------------------------------------------------------------------- */

#if 1

    ConversionResult ConvertUTF32toUTF16(
            const UTF32** sourceStart, const UTF32* sourceEnd,
            UTF16** targetStart, UTF16* targetEnd, ConversionFlags flags) {
        ConversionResult result = conversionOK;
        const UTF32* source = *sourceStart;
        UTF16* target = *targetStart;
        while (source < sourceEnd) {
            UTF32 ch;
            if (target >= targetEnd) {
                result = targetExhausted;
                break;
            }
            ch = *source++;
            if (ch <= UNI_MAX_BMP) { /* Target is a character <= 0xFFFF */
                /* UTF-16 surrogate values are illegal in UTF-32; 0xffff or 0xfffe are both reserved values */
                if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
                    if (flags == strictConversion) {
                        --source; /* return to the illegal value itself */
                        result = sourceIllegal;
                        break;
                    } else {
                        *target++ = UNI_REPLACEMENT_CHAR;
                    }
                } else {
                    *target++ = (UTF16) ch; /* normal case */
                }
            } else if (ch > UNI_MAX_LEGAL_UTF32) {
                if (flags == strictConversion) {
                    result = sourceIllegal;
                } else {
                    *target++ = UNI_REPLACEMENT_CHAR;
                }
            } else {
                /* target is a character in range 0xFFFF - 0x10FFFF. */
                if (target + 1 >= targetEnd) {
                    --source; /* Back up source pointer! */
                    result = targetExhausted;
                    break;
                }
                ch -= halfBase;
                *target++ = (UTF16) ((ch >> halfShift) + UNI_SUR_HIGH_START);
                *target++ = (UTF16) ((ch & halfMask) + UNI_SUR_LOW_START);
            }
        }
        *sourceStart = source;
        *targetStart = target;
        return result;
    }

    /* --------------------------------------------------------------------- */

    ConversionResult ConvertUTF16toUTF32(
            const UTF16** sourceStart, const UTF16* sourceEnd,
            UTF32** targetStart, UTF32* targetEnd, ConversionFlags flags) {
        ConversionResult result = conversionOK;
        const UTF16* source = *sourceStart;
        UTF32* target = *targetStart;
        UTF32 ch, ch2;
        while (source < sourceEnd) {
            const UTF16* oldSource = source; /*  In case we have to back up because of target overflow. */
            ch = *source++;
            /* If we have a surrogate pair, convert to UTF32 first. */
            if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END) {
                /* If the 16 bits following the high surrogate are in the source buffer... */
                if (source < sourceEnd) {
                    ch2 = *source;
                    /* If it's a low surrogate, convert to UTF32. */
                    if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
                        ch = ((ch - UNI_SUR_HIGH_START) << halfShift)
                                + (ch2 - UNI_SUR_LOW_START) + halfBase;
                        ++source;
                    } else if (flags == strictConversion) { /* it's an unpaired high surrogate */
                        --source; /* return to the illegal value itself */
                        result = sourceIllegal;
                        break;
                    }
                } else { /* We don't have the 16 bits following the high surrogate. */
                    --source; /* return to the high surrogate */
                    result = sourceExhausted;
                    break;
                }
            } else if (flags == strictConversion) {
                /* UTF-16 surrogate values are illegal in UTF-32 */
                if (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END) {
                    --source; /* return to the illegal value itself */
                    result = sourceIllegal;
                    break;
                }
            }
            if (target >= targetEnd) {
                source = oldSource; /* Back up source pointer! */
                result = targetExhausted;
                break;
            }
            *target++ = ch;
        }
        *sourceStart = source;
        *targetStart = target;
#ifdef CVTUTF_DEBUG
        if (result == sourceIllegal) {
            fprintf(stderr, "ConvertUTF16toUTF32 illegal seq 0x%04x,%04x\n", ch, ch2);
            fflush(stderr);
        }
#endif
        return result;
    }

    /* --------------------------------------------------------------------- */

    /*
     * Index into the table below with the first byte of a UTF-8 sequence to
     * get the number of trailing bytes that are supposed to follow it.
     * Note that *legal* UTF-8 values can't have 4 or 5-bytes. The table is
     * left as-is for anyone who may want to do such conversion, which was
     * allowed in earlier algorithms.
     */
    static const char trailingBytesForUTF8[256] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5
    };

    /*
     * Magic values subtracted from a buffer value during UTF8 conversion.
     * This table contains as many values as there might be trailing bytes
     * in a UTF-8 sequence.
     */
    static const UTF32 offsetsFromUTF8[6] = {0x00000000UL, 0x00003080UL, 0x000E2080UL,
        0x03C82080UL, 0xFA082080UL, 0x82082080UL};

    /*
     * Once the bits are split out into bytes of UTF-8, this is a mask OR-ed
     * into the first byte, depending on how many bytes follow.  There are
     * as many entries in this table as there are UTF-8 sequence types.
     * (I.e., one byte sequence, two byte... etc.). Remember that sequencs
     * for *legal* UTF-8 will be 4 or fewer bytes total.
     */
    static const UTF8 firstByteMark[7] = {0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC};

    /* --------------------------------------------------------------------- */

    /* The interface converts a whole buffer to avoid function-call overhead.
     * Constants have been gathered. Loops & conditionals have been removed as
     * much as possible for efficiency, in favor of drop-through switches.
     * (See "Note A" at the bottom of the file for equivalent code.)
     * If your compiler supports it, the "isLegalUTF8" call can be turned
     * into an inline function.
     */

    /* --------------------------------------------------------------------- */

    ConversionResult ConvertUTF16toUTF8(
            const UTF16** sourceStart, const UTF16* sourceEnd,
            UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags) {
        ConversionResult result = conversionOK;
        const UTF16* source = *sourceStart;
        UTF8* target = *targetStart;
        while (source < sourceEnd) {
            UTF32 ch;
            unsigned short bytesToWrite = 0;
            const UTF32 byteMask = 0xBF;
            const UTF32 byteMark = 0x80;
            const UTF16* oldSource = source; /* In case we have to back up because of target overflow. */
            ch = *source++;
            /* If we have a surrogate pair, convert to UTF32 first. */
            if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_HIGH_END) {
                /* If the 16 bits following the high surrogate are in the source buffer... */
                if (source < sourceEnd) {
                    UTF32 ch2 = *source;
                    /* If it's a low surrogate, convert to UTF32. */
                    if (ch2 >= UNI_SUR_LOW_START && ch2 <= UNI_SUR_LOW_END) {
                        ch = ((ch - UNI_SUR_HIGH_START) << halfShift)
                                + (ch2 - UNI_SUR_LOW_START) + halfBase;
                        ++source;
                    } else if (flags == strictConversion) { /* it's an unpaired high surrogate */
                        --source; /* return to the illegal value itself */
                        result = sourceIllegal;
                        break;
                    }
                } else { /* We don't have the 16 bits following the high surrogate. */
                    --source; /* return to the high surrogate */
                    result = sourceExhausted;
                    break;
                }
            } else if (flags == strictConversion) {
                /* UTF-16 surrogate values are illegal in UTF-32 */
                if (ch >= UNI_SUR_LOW_START && ch <= UNI_SUR_LOW_END) {
                    --source; /* return to the illegal value itself */
                    result = sourceIllegal;
                    break;
                }
            }
            /* Figure out how many bytes the result will require */
            if (ch < (UTF32) 0x80) {
                bytesToWrite = 1;
            } else if (ch < (UTF32) 0x800) {
                bytesToWrite = 2;
            } else if (ch < (UTF32) 0x10000) {
                bytesToWrite = 3;
            } else if (ch < (UTF32) 0x110000) {
                bytesToWrite = 4;
            } else {
                bytesToWrite = 3;
                ch = UNI_REPLACEMENT_CHAR;
            }

            target += bytesToWrite;
            if (target > targetEnd) {
                source = oldSource; /* Back up source pointer! */
                target -= bytesToWrite;
                result = targetExhausted;
                break;
            }
            switch (bytesToWrite) { /* note: everything falls through. */
                case 4: *--target = (UTF8) ((ch | byteMark) & byteMask);
                    ch >>= 6;
                case 3: *--target = (UTF8) ((ch | byteMark) & byteMask);
                    ch >>= 6;
                case 2: *--target = (UTF8) ((ch | byteMark) & byteMask);
                    ch >>= 6;
                case 1: *--target = (UTF8) (ch | firstByteMark[bytesToWrite]);
            }
            target += bytesToWrite;
        }
        *sourceStart = source;
        *targetStart = target;
        return result;
    }

    /* --------------------------------------------------------------------- */

    /*
     * Utility routine to tell whether a sequence of bytes is legal UTF-8.
     * This must be called with the length pre-determined by the first byte.
     * If not calling this from ConvertUTF8to*, then the length can be set by:
     *  length = trailingBytesForUTF8[*source]+1;
     * and the sequence is illegal right away if there aren't that many bytes
     * available.
     * If presented with a length > 4, this returns false.  The Unicode
     * definition of UTF-8 goes up to 4-byte sequences.
     */

    static bool isLegalUTF8(const UTF8 *source, int length) {
        UTF8 a;
        const UTF8 *srcptr = source + length;
        switch (length) {
            default: return false;
                /* Everything else falls through when "true"... */
            case 4: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;
            case 3: if ((a = (*--srcptr)) < 0x80 || a > 0xBF) return false;
            case 2: if ((a = (*--srcptr)) > 0xBF) return false;

                switch (*source) {
                        /* no fall-through in this inner switch */
                    case 0xE0: if (a < 0xA0) return false;
                        break;
                    case 0xED: if (a > 0x9F) return false;
                        break;
                    case 0xF0: if (a < 0x90) return false;
                        break;
                    case 0xF4: if (a > 0x8F) return false;
                        break;
                    default: if (a < 0x80) return false;
                }

            case 1: if (*source >= 0x80 && *source < 0xC2) return false;
        }
        if (*source > 0xF4) return false;
        return true;
    }

    /* --------------------------------------------------------------------- */

    /*
     * Exported function to return whether a UTF-8 sequence is legal or not.
     * This is not used here; it's just exported.
     */
    bool isLegalUTF8Sequence(const UTF8 *source, const UTF8 *sourceEnd) {
        int length = trailingBytesForUTF8[*source] + 1;
        if (source + length > sourceEnd) {
            return false;
        }
        return isLegalUTF8(source, length);
    }

    /* --------------------------------------------------------------------- */

    ConversionResult ConvertUTF8toUTF16(
            const UTF8** sourceStart, const UTF8* sourceEnd,
            UTF16** targetStart, UTF16* targetEnd, ConversionFlags flags) {
        ConversionResult result = conversionOK;
        const UTF8* source = *sourceStart;
        UTF16* target = *targetStart;
        while (source < sourceEnd) {
            UTF32 ch = 0;
            unsigned short extraBytesToRead = trailingBytesForUTF8[*source];
            if (source + extraBytesToRead >= sourceEnd) {
                result = sourceExhausted;
                break;
            }
            /* Do this check whether lenient or strict */
            if (!isLegalUTF8(source, extraBytesToRead + 1)) {
                result = sourceIllegal;
                break;
            }
            /*
             * The cases all fall through. See "Note A" below.
             */
            switch (extraBytesToRead) {
                case 5: ch += *source++;
                    ch <<= 6; /* remember, illegal UTF-8 */
                case 4: ch += *source++;
                    ch <<= 6; /* remember, illegal UTF-8 */
                case 3: ch += *source++;
                    ch <<= 6;
                case 2: ch += *source++;
                    ch <<= 6;
                case 1: ch += *source++;
                    ch <<= 6;
                case 0: ch += *source++;
            }
            ch -= offsetsFromUTF8[extraBytesToRead];

            if (target >= targetEnd) {
                source -= (extraBytesToRead + 1); /* Back up source pointer! */
                result = targetExhausted;
                break;
            }
            if (ch <= UNI_MAX_BMP) { /* Target is a character <= 0xFFFF */
                /* UTF-16 surrogate values are illegal in UTF-32 */
                if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
                    if (flags == strictConversion) {
                        source -= (extraBytesToRead + 1); /* return to the illegal value itself */
                        result = sourceIllegal;
                        break;
                    } else {
                        *target++ = UNI_REPLACEMENT_CHAR;
                    }
                } else {
                    *target++ = (UTF16) ch; /* normal case */
                }
            } else if (ch > UNI_MAX_UTF16) {
                if (flags == strictConversion) {
                    result = sourceIllegal;
                    source -= (extraBytesToRead + 1); /* return to the start */
                    break; /* Bail out; shouldn't continue */
                } else {
                    *target++ = UNI_REPLACEMENT_CHAR;
                }
            } else {
                /* target is a character in range 0xFFFF - 0x10FFFF. */
                if (target + 1 >= targetEnd) {
                    source -= (extraBytesToRead + 1); /* Back up source pointer! */
                    result = targetExhausted;
                    break;
                }
                ch -= halfBase;
                *target++ = (UTF16) ((ch >> halfShift) + UNI_SUR_HIGH_START);
                *target++ = (UTF16) ((ch & halfMask) + UNI_SUR_LOW_START);
            }
        }
        *sourceStart = source;
        *targetStart = target;
        return result;
    }

    /* --------------------------------------------------------------------- */

    ConversionResult ConvertUTF32toUTF8(
            const UTF32** sourceStart, const UTF32* sourceEnd,
            UTF8** targetStart, UTF8* targetEnd, ConversionFlags flags) {
        ConversionResult result = conversionOK;
        const UTF32* source = *sourceStart;
        UTF8* target = *targetStart;
        while (source < sourceEnd) {
            UTF32 ch;
            unsigned short bytesToWrite = 0;
            const UTF32 byteMask = 0xBF;
            const UTF32 byteMark = 0x80;
            ch = *source++;
            if (flags == strictConversion) {
                /* UTF-16 surrogate values are illegal in UTF-32 */
                if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
                    --source; /* return to the illegal value itself */
                    result = sourceIllegal;
                    break;
                }
            }
            /*
             * Figure out how many bytes the result will require. Turn any
             * illegally large UTF32 things (> Plane 17) into replacement chars.
             */
            if (ch < (UTF32) 0x80) {
                bytesToWrite = 1;
            } else if (ch < (UTF32) 0x800) {
                bytesToWrite = 2;
            } else if (ch < (UTF32) 0x10000) {
                bytesToWrite = 3;
            } else if (ch <= UNI_MAX_LEGAL_UTF32) {
                bytesToWrite = 4;
            } else {
                bytesToWrite = 3;
                ch = UNI_REPLACEMENT_CHAR;
                result = sourceIllegal;
            }

            target += bytesToWrite;
            if (target > targetEnd) {
                --source; /* Back up source pointer! */
                target -= bytesToWrite;
                result = targetExhausted;
                break;
            }
            switch (bytesToWrite) { /* note: everything falls through. */
                case 4: *--target = (UTF8) ((ch | byteMark) & byteMask);
                    ch >>= 6;
                case 3: *--target = (UTF8) ((ch | byteMark) & byteMask);
                    ch >>= 6;
                case 2: *--target = (UTF8) ((ch | byteMark) & byteMask);
                    ch >>= 6;
                case 1: *--target = (UTF8) (ch | firstByteMark[bytesToWrite]);
            }
            target += bytesToWrite;
        }
        *sourceStart = source;
        *targetStart = target;
        return result;
    }

    /* --------------------------------------------------------------------- */

    ConversionResult ConvertUTF8toUTF32(
            const UTF8** sourceStart, const UTF8* sourceEnd,
            UTF32** targetStart, UTF32* targetEnd, ConversionFlags flags) {
        ConversionResult result = conversionOK;
        const UTF8* source = *sourceStart;
        UTF32* target = *targetStart;
        while (source < sourceEnd) {
            UTF32 ch = 0;
            unsigned short extraBytesToRead = trailingBytesForUTF8[*source];
            if (source + extraBytesToRead >= sourceEnd) {
                result = sourceExhausted;
                break;
            }
            /* Do this check whether lenient or strict */
            if (!isLegalUTF8(source, extraBytesToRead + 1)) {
                result = sourceIllegal;
                break;
            }
            /*
             * The cases all fall through. See "Note A" below.
             */
            switch (extraBytesToRead) {
                case 5: ch += *source++;
                    ch <<= 6;
                case 4: ch += *source++;
                    ch <<= 6;
                case 3: ch += *source++;
                    ch <<= 6;
                case 2: ch += *source++;
                    ch <<= 6;
                case 1: ch += *source++;
                    ch <<= 6;
                case 0: ch += *source++;
            }
            ch -= offsetsFromUTF8[extraBytesToRead];

            if (target >= targetEnd) {
                source -= (extraBytesToRead + 1); /* Back up the source pointer! */
                result = targetExhausted;
                break;
            }
            if (ch <= UNI_MAX_LEGAL_UTF32) {
                /*
                 * UTF-16 surrogate values are illegal in UTF-32, and anything
                 * over Plane 17 (> 0x10FFFF) is illegal.
                 */
                if (ch >= UNI_SUR_HIGH_START && ch <= UNI_SUR_LOW_END) {
                    if (flags == strictConversion) {
                        source -= (extraBytesToRead + 1); /* return to the illegal value itself */
                        result = sourceIllegal;
                        break;
                    } else {
                        *target++ = UNI_REPLACEMENT_CHAR;
                    }
                } else {
                    *target++ = ch;
                }
            } else { /* i.e., ch > UNI_MAX_LEGAL_UTF32 */
                result = sourceIllegal;
                *target++ = UNI_REPLACEMENT_CHAR;
            }
        }
        *sourceStart = source;
        *targetStart = target;
        return result;
    }
#endif
    /* ---------------------------------------------------------------------

        Note A.
        The fall-through switches in UTF-8 reading code save a
        temp variable, some decrements & conditionals.  The switches
        are equivalent to the following loop:
            {
                int tmpBytesToRead = extraBytesToRead+1;
                do {
                    ch += *source++;
                    --tmpBytesToRead;
                    if (tmpBytesToRead) ch <<= 6;
                } while (tmpBytesToRead > 0);
            }
        In UTF-8 writing code, the switches on "bytesToWrite" are
        similarly unrolled loops.

       --------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

/* --------------------------------------------------------------------- */

/**
 * Converts UTF-8 to a wchar_t (or equivalent) using the Unicode reference
 * library functions. This can be used on all platforms.
 */
template<class SI_CHAR>
class SI_ConvertW {
    bool m_bStoreIsUtf8;
protected:

    SI_ConvertW() {
    }
public:

    SI_ConvertW(bool a_bStoreIsUtf8) : m_bStoreIsUtf8(a_bStoreIsUtf8) {
    }

    /* copy and assignment */
    SI_ConvertW(const SI_ConvertW & rhs) {
        operator=(rhs);
    }

    SI_ConvertW & operator=(const SI_ConvertW & rhs) {
        m_bStoreIsUtf8 = rhs.m_bStoreIsUtf8;
        return *this;
    }

    /** Calculate the number of SI_CHAR required for converting the input
     * from the storage format. The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  Data in storage format to be converted to SI_CHAR.
     * @param a_uInputDataLen Length of storage format data in bytes. This
     *                      must be the actual length of the data, including
     *                      NULL byte if NULL terminated string is required.
     * @return              Number of SI_CHAR required by the string when
     *                      converted. If there are embedded NULL bytes in the
     *                      input data, only the string up and not including
     *                      the NULL byte will be converted.
     * @return              -1 cast to size_t on a conversion error.
     */
    size_t SizeFromStore(
            const char * a_pInputData,
            size_t a_uInputDataLen) {
        SI_ASSERT(a_uInputDataLen != (size_t) - 1);

        if (m_bStoreIsUtf8) {
            // worst case scenario for UTF-8 to wchar_t is 1 char -> 1 wchar_t
            // so we just return the same number of characters required as for
            // the source text.
            return a_uInputDataLen;
        }

#if defined(SI_NO_MBSTOWCS_NULL) || (!defined(_MSC_VER) && !defined(_linux))
        // fall back processing for platforms that don't support a NULL dest to mbstowcs
        // worst case scenario is 1:1, this will be a sufficient buffer size
        (void) a_pInputData;
        return a_uInputDataLen;
#else
        // get the actual required buffer size
        return mbstowcs(NULL, a_pInputData, a_uInputDataLen);
#endif
    }

    /** Convert the input string from the storage format to SI_CHAR.
     * The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  Data in storage format to be converted to SI_CHAR.
     * @param a_uInputDataLen Length of storage format data in bytes. This
     *                       must be the actual length of the data, including
     *                       NULL byte if NULL terminated string is required.
     * @param a_pOutputData Pointer to the output buffer to received the
     *                       converted data.
     * @param a_uOutputDataSize Size of the output buffer in SI_CHAR.
     * @return              true if all of the input data was successfully
     *                       converted.
     */
    bool ConvertFromStore(
            const char * a_pInputData,
            size_t a_uInputDataLen,
            SI_CHAR * a_pOutputData,
            size_t a_uOutputDataSize) {
        if (m_bStoreIsUtf8) {
            // This uses the Unicode reference implementation to do the
            // conversion from UTF-8 to wchar_t. The required files are
            // ConvertUTF.h and ConvertUTF.c which should be included in
            // the distribution but are publically available from unicode.org
            // at http://www.unicode.org/Public/PROGRAMS/CVTUTF/
            ConversionResult retval;
            const UTF8 * pUtf8 = (const UTF8 *) a_pInputData;
            if (sizeof (wchar_t) == sizeof (UTF32)) {
                UTF32 * pUtf32 = (UTF32 *) a_pOutputData;
                retval = ConvertUTF8toUTF32(
                        &pUtf8, pUtf8 + a_uInputDataLen,
                        &pUtf32, pUtf32 + a_uOutputDataSize,
                        lenientConversion);
            } else if (sizeof (wchar_t) == sizeof (UTF16)) {
                UTF16 * pUtf16 = (UTF16 *) a_pOutputData;
                retval = ConvertUTF8toUTF16(
                        &pUtf8, pUtf8 + a_uInputDataLen,
                        &pUtf16, pUtf16 + a_uOutputDataSize,
                        lenientConversion);
            }
            return retval == conversionOK;
        }

        // convert to wchar_t
        size_t retval = mbstowcs(a_pOutputData,
                a_pInputData, a_uOutputDataSize);
        return retval != (size_t) (-1);
    }

    /** Calculate the number of char required by the storage format of this
     * data. The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  NULL terminated string to calculate the number of
     *                       bytes required to be converted to storage format.
     * @return              Number of bytes required by the string when
     *                       converted to storage format. This size always
     *                       includes space for the terminating NULL character.
     * @return              -1 cast to size_t on a conversion error.
     */
    size_t SizeToStore(
            const SI_CHAR * a_pInputData) {
        if (m_bStoreIsUtf8) {
            // worst case scenario for wchar_t to UTF-8 is 1 wchar_t -> 6 char
            size_t uLen = 0;
            while (a_pInputData[uLen]) {
                ++uLen;
            }
            return (6 * uLen) + 1;
        } else {
            size_t uLen = wcstombs(NULL, a_pInputData, 0);
            if (uLen == (size_t) (-1)) {
                return uLen;
            }
            return uLen + 1; // include NULL terminator
        }
    }

    /** Convert the input string to the storage format of this data.
     * The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  NULL terminated source string to convert. All of
     *                       the data will be converted including the
     *                       terminating NULL character.
     * @param a_pOutputData Pointer to the buffer to receive the converted
     *                       string.
     * @param a_uOutputDataSize Size of the output buffer in char.
     * @return              true if all of the input data, including the
     *                       terminating NULL character was successfully
     *                       converted.
     */
    bool ConvertToStore(
            const SI_CHAR * a_pInputData,
            char * a_pOutputData,
            size_t a_uOutputDataSize
            ) {
        if (m_bStoreIsUtf8) {
            // calc input string length (SI_CHAR type and size independent)
            size_t uInputLen = 0;
            while (a_pInputData[uInputLen]) {
                ++uInputLen;
            }
            ++uInputLen; // include the NULL char

            // This uses the Unicode reference implementation to do the
            // conversion from wchar_t to UTF-8. The required files are
            // ConvertUTF.h and ConvertUTF.c which should be included in
            // the distribution but are publically available from unicode.org
            // at http://www.unicode.org/Public/PROGRAMS/CVTUTF/
            ConversionResult retval;
            UTF8 * pUtf8 = (UTF8 *) a_pOutputData;
            if (sizeof (wchar_t) == sizeof (UTF32)) {
                const UTF32 * pUtf32 = (const UTF32 *) a_pInputData;
                retval = ConvertUTF32toUTF8(
                        &pUtf32, pUtf32 + uInputLen,
                        &pUtf8, pUtf8 + a_uOutputDataSize,
                        lenientConversion);
            } else if (sizeof (wchar_t) == sizeof (UTF16)) {
                const UTF16 * pUtf16 = (const UTF16 *) a_pInputData;
                retval = ConvertUTF16toUTF8(
                        &pUtf16, pUtf16 + uInputLen,
                        &pUtf8, pUtf8 + a_uOutputDataSize,
                        lenientConversion);
            }
            return retval == conversionOK;
        } else {
            size_t retval = wcstombs(a_pOutputData,
                    a_pInputData, a_uOutputDataSize);
            return retval != (size_t) - 1;
        }
    }
};

#endif // SI_CONVERT_GENERIC


// ---------------------------------------------------------------------------
//                              SI_CONVERT_ICU
// ---------------------------------------------------------------------------
#ifdef SI_CONVERT_ICU

#define SI_Case     SI_GenericCase
#define SI_NoCase   SI_GenericNoCase

#include <unicode/ucnv.h>

/**
 * Converts MBCS/UTF-8 to UChar using ICU. This can be used on all platforms.
 */
template<class SI_CHAR>
class SI_ConvertW {
    const char * m_pEncoding;
    UConverter * m_pConverter;
protected:

    SI_ConvertW() : m_pEncoding(NULL), m_pConverter(NULL) {
    }
public:

    SI_ConvertW(bool a_bStoreIsUtf8) : m_pConverter(NULL) {
        m_pEncoding = a_bStoreIsUtf8 ? "UTF-8" : NULL;
    }

    /* copy and assignment */
    SI_ConvertW(const SI_ConvertW & rhs) {
        operator=(rhs);
    }

    SI_ConvertW & operator=(const SI_ConvertW & rhs) {
        m_pEncoding = rhs.m_pEncoding;
        m_pConverter = NULL;
        return *this;
    }

    ~SI_ConvertW() {
        if (m_pConverter) ucnv_close(m_pConverter);
    }

    /** Calculate the number of UChar required for converting the input
     * from the storage format. The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  Data in storage format to be converted to UChar.
     * @param a_uInputDataLen Length of storage format data in bytes. This
     *                      must be the actual length of the data, including
     *                      NULL byte if NULL terminated string is required.
     * @return              Number of UChar required by the string when
     *                      converted. If there are embedded NULL bytes in the
     *                      input data, only the string up and not including
     *                      the NULL byte will be converted.
     * @return              -1 cast to size_t on a conversion error.
     */
    size_t SizeFromStore(
            const char * a_pInputData,
            size_t a_uInputDataLen) {
        SI_ASSERT(a_uInputDataLen != (size_t) - 1);

        UErrorCode nError;

        if (!m_pConverter) {
            nError = U_ZERO_ERROR;
            m_pConverter = ucnv_open(m_pEncoding, &nError);
            if (U_FAILURE(nError)) {
                return (size_t) - 1;
            }
        }

        nError = U_ZERO_ERROR;
        int32_t nLen = ucnv_toUChars(m_pConverter, NULL, 0,
                a_pInputData, (int32_t) a_uInputDataLen, &nError);
        if (U_FAILURE(nError) && nError != U_BUFFER_OVERFLOW_ERROR) {
            return (size_t) - 1;
        }

        return (size_t) nLen;
    }

    /** Convert the input string from the storage format to UChar.
     * The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  Data in storage format to be converted to UChar.
     * @param a_uInputDataLen Length of storage format data in bytes. This
     *                      must be the actual length of the data, including
     *                      NULL byte if NULL terminated string is required.
     * @param a_pOutputData Pointer to the output buffer to received the
     *                      converted data.
     * @param a_uOutputDataSize Size of the output buffer in UChar.
     * @return              true if all of the input data was successfully
     *                      converted.
     */
    bool ConvertFromStore(
            const char * a_pInputData,
            size_t a_uInputDataLen,
            UChar * a_pOutputData,
            size_t a_uOutputDataSize) {
        UErrorCode nError;

        if (!m_pConverter) {
            nError = U_ZERO_ERROR;
            m_pConverter = ucnv_open(m_pEncoding, &nError);
            if (U_FAILURE(nError)) {
                return false;
            }
        }

        nError = U_ZERO_ERROR;
        ucnv_toUChars(m_pConverter,
                a_pOutputData, (int32_t) a_uOutputDataSize,
                a_pInputData, (int32_t) a_uInputDataLen, &nError);
        if (U_FAILURE(nError)) {
            return false;
        }

        return true;
    }

    /** Calculate the number of char required by the storage format of this
     * data. The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  NULL terminated string to calculate the number of
     *                      bytes required to be converted to storage format.
     * @return              Number of bytes required by the string when
     *                      converted to storage format. This size always
     *                      includes space for the terminating NULL character.
     * @return              -1 cast to size_t on a conversion error.
     */
    size_t SizeToStore(
            const UChar * a_pInputData) {
        UErrorCode nError;

        if (!m_pConverter) {
            nError = U_ZERO_ERROR;
            m_pConverter = ucnv_open(m_pEncoding, &nError);
            if (U_FAILURE(nError)) {
                return (size_t) - 1;
            }
        }

        nError = U_ZERO_ERROR;
        int32_t nLen = ucnv_fromUChars(m_pConverter, NULL, 0,
                a_pInputData, -1, &nError);
        if (U_FAILURE(nError) && nError != U_BUFFER_OVERFLOW_ERROR) {
            return (size_t) - 1;
        }

        return (size_t) nLen + 1;
    }

    /** Convert the input string to the storage format of this data.
     * The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  NULL terminated source string to convert. All of
     *                      the data will be converted including the
     *                      terminating NULL character.
     * @param a_pOutputData Pointer to the buffer to receive the converted
     *                      string.
     * @param a_uOutputDataSize Size of the output buffer in char.
     * @return              true if all of the input data, including the
     *                      terminating NULL character was successfully
     *                      converted.
     */
    bool ConvertToStore(
            const UChar * a_pInputData,
            char * a_pOutputData,
            size_t a_uOutputDataSize) {
        UErrorCode nError;

        if (!m_pConverter) {
            nError = U_ZERO_ERROR;
            m_pConverter = ucnv_open(m_pEncoding, &nError);
            if (U_FAILURE(nError)) {
                return false;
            }
        }

        nError = U_ZERO_ERROR;
        ucnv_fromUChars(m_pConverter,
                a_pOutputData, (int32_t) a_uOutputDataSize,
                a_pInputData, -1, &nError);
        if (U_FAILURE(nError)) {
            return false;
        }

        return true;
    }
};

#endif // SI_CONVERT_ICU


// ---------------------------------------------------------------------------
//                              SI_CONVERT_WIN32
// ---------------------------------------------------------------------------
#ifdef SI_CONVERT_WIN32

#define SI_Case     SI_GenericCase

// Windows CE doesn't have errno or MBCS libraries
#ifdef _WIN32_WCE
#ifndef SI_NO_MBCS
#define SI_NO_MBCS
#endif
#endif

#include <windows.h>
#ifdef SI_NO_MBCS
#define SI_NoCase   SI_GenericNoCase
#else // !SI_NO_MBCS
/**
 * Case-insensitive comparison class using Win32 MBCS functions. This class
 * returns a case-insensitive semi-collation order for MBCS text. It may not
 * be safe for UTF-8 text returned in char format as we don't know what
 * characters will be folded by the function! Therefore, if you are using
 * SI_CHAR == char and SetUnicode(true), then you need to use the generic
 * SI_NoCase class instead.
 */
#include <mbstring.h>

template<class SI_CHAR>
struct SI_NoCase {

    bool operator()(const SI_CHAR * pLeft, const SI_CHAR * pRight) const {
        if (sizeof (SI_CHAR) == sizeof (char)) {
            return _mbsicmp((const unsigned char *) pLeft,
                    (const unsigned char *) pRight) < 0;
        }
        if (sizeof (SI_CHAR) == sizeof (wchar_t)) {
            return _wcsicmp((const wchar_t *)pLeft,
                    (const wchar_t *)pRight) < 0;
        }
        return SI_GenericNoCase<SI_CHAR>()(pLeft, pRight);
    }
};
#endif // SI_NO_MBCS

/**
 * Converts MBCS and UTF-8 to a wchar_t (or equivalent) on Windows. This uses
 * only the Win32 functions and doesn't require the external Unicode UTF-8
 * conversion library. It will not work on Windows 95 without using Microsoft
 * Layer for Unicode in your application.
 */
template<class SI_CHAR>
class SI_ConvertW {
    UINT m_uCodePage;
protected:

    SI_ConvertW() {
    }
public:

    SI_ConvertW(bool a_bStoreIsUtf8) {
        m_uCodePage = a_bStoreIsUtf8 ? CP_UTF8 : CP_ACP;
    }

    /* copy and assignment */
    SI_ConvertW(const SI_ConvertW & rhs) {
        operator=(rhs);
    }

    SI_ConvertW & operator=(const SI_ConvertW & rhs) {
        m_uCodePage = rhs.m_uCodePage;
        return *this;
    }

    /** Calculate the number of SI_CHAR required for converting the input
     * from the storage format. The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  Data in storage format to be converted to SI_CHAR.
     * @param a_uInputDataLen Length of storage format data in bytes. This
     *                      must be the actual length of the data, including
     *                      NULL byte if NULL terminated string is required.
     * @return              Number of SI_CHAR required by the string when
     *                      converted. If there are embedded NULL bytes in the
     *                      input data, only the string up and not including
     *                      the NULL byte will be converted.
     * @return              -1 cast to size_t on a conversion error.
     */
    size_t SizeFromStore(
            const char * a_pInputData,
            size_t a_uInputDataLen) {
        SI_ASSERT(a_uInputDataLen != (size_t) - 1);

        int retval = MultiByteToWideChar(
                m_uCodePage, 0,
                a_pInputData, (int) a_uInputDataLen,
                0, 0);
        return (size_t) (retval > 0 ? retval : -1);
    }

    /** Convert the input string from the storage format to SI_CHAR.
     * The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  Data in storage format to be converted to SI_CHAR.
     * @param a_uInputDataLen Length of storage format data in bytes. This
     *                      must be the actual length of the data, including
     *                      NULL byte if NULL terminated string is required.
     * @param a_pOutputData Pointer to the output buffer to received the
     *                      converted data.
     * @param a_uOutputDataSize Size of the output buffer in SI_CHAR.
     * @return              true if all of the input data was successfully
     *                      converted.
     */
    bool ConvertFromStore(
            const char * a_pInputData,
            size_t a_uInputDataLen,
            SI_CHAR * a_pOutputData,
            size_t a_uOutputDataSize) {
        int nSize = MultiByteToWideChar(
                m_uCodePage, 0,
                a_pInputData, (int) a_uInputDataLen,
                (wchar_t *) a_pOutputData, (int) a_uOutputDataSize);
        return (nSize > 0);
    }

    /** Calculate the number of char required by the storage format of this
     * data. The storage format is always UTF-8.
     *
     * @param a_pInputData  NULL terminated string to calculate the number of
     *                      bytes required to be converted to storage format.
     * @return              Number of bytes required by the string when
     *                      converted to storage format. This size always
     *                      includes space for the terminating NULL character.
     * @return              -1 cast to size_t on a conversion error.
     */
    size_t SizeToStore(
            const SI_CHAR * a_pInputData) {
        int retval = WideCharToMultiByte(
                m_uCodePage, 0,
                (const wchar_t *) a_pInputData, -1,
                0, 0, 0, 0);
        return (size_t) (retval > 0 ? retval : -1);
    }

    /** Convert the input string to the storage format of this data.
     * The storage format is always UTF-8 or MBCS.
     *
     * @param a_pInputData  NULL terminated source string to convert. All of
     *                      the data will be converted including the
     *                      terminating NULL character.
     * @param a_pOutputData Pointer to the buffer to receive the converted
     *                      string.
     * @param a_uOutputDataSize Size of the output buffer in char.
     * @return              true if all of the input data, including the
     *                      terminating NULL character was successfully
     *                      converted.
     */
    bool ConvertToStore(
            const SI_CHAR * a_pInputData,
            char * a_pOutputData,
            size_t a_uOutputDataSize) {
        int retval = WideCharToMultiByte(
                m_uCodePage, 0,
                (const wchar_t *) a_pInputData, -1,
                a_pOutputData, (int) a_uOutputDataSize, 0, 0);
        return retval > 0;
    }
};

#endif // SI_CONVERT_WIN32


// ---------------------------------------------------------------------------
//                                  TYPE DEFINITIONS
// ---------------------------------------------------------------------------

typedef CSimpleIniTempl<char,
SI_NoCase<char>, SI_ConvertA<char> > CSimpleIniA;
typedef CSimpleIniTempl<char,
SI_Case<char>, SI_ConvertA<char> > CSimpleIniCaseA;

#if defined(SI_CONVERT_ICU)
typedef CSimpleIniTempl<UChar,
SI_NoCase<UChar>, SI_ConvertW<UChar> > CSimpleIniW;
typedef CSimpleIniTempl<UChar,
SI_Case<UChar>, SI_ConvertW<UChar> > CSimpleIniCaseW;
#else
typedef CSimpleIniTempl<wchar_t,
SI_NoCase<wchar_t>, SI_ConvertW<wchar_t> > CSimpleIniW;
typedef CSimpleIniTempl<wchar_t,
SI_Case<wchar_t>, SI_ConvertW<wchar_t> > CSimpleIniCaseW;
#endif

#ifdef _UNICODE
#define CSimpleIni      CSimpleIniW
#define CSimpleIniCase  CSimpleIniCaseW
#define SI_NEWLINE      SI_NEWLINE_W
#else // !_UNICODE
#define CSimpleIni      CSimpleIniA
#define CSimpleIniCase  CSimpleIniCaseA
#define SI_NEWLINE      SI_NEWLINE_A
#endif // _UNICODE

#ifdef _MSC_VER
#pragma warning (pop)
#endif

#endif // INCLUDED_SimpleIni_h


///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2018, STEREOLABS.
//
// All rights reserved.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

inline std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

inline std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}

class ConfManager {
public:

    ConfManager(std::string filename) {
        filename_ = filename;
        ini_.SetUnicode();
        SI_Error rc = ini_.LoadFile(filename_.c_str());
        is_opened_ = !(rc < 0);
    }

    ~ConfManager() {
        //if (is_opened_) ini_.SaveFile(filename_.c_str());
    }

    float getValue(std::string key, float default_value = -1) {
        if (is_opened_) {
            std::vector<std::string> elems;
            split(key, ':', elems);

            return atof(ini_.GetValue(elems.front().c_str(), elems.back().c_str(), std::to_string(default_value).c_str()));
        } else
            return -1.f;
    }

    void setValue(std::string key, float value) {
        if (is_opened_) {
            std::vector<std::string> elems;
            split(key, ':', elems);

            /*SI_Error rc = */ini_.SetValue(elems.front().c_str(), elems.back().c_str(), std::to_string(value).c_str());
        }
    }

    inline bool isOpened() {
        return is_opened_;
    }

private:
    std::string filename_;
    bool is_opened_;
    CSimpleIniA ini_;
};



bool checkFile(std::string path) {
    std::ifstream f(path.c_str());
    return f.good();
}

static inline std::string getRootHiddenDir() {
#ifdef WIN32

#ifdef UNICODE
    wchar_t szPath[MAX_PATH];
#else
    TCHAR szPath[MAX_PATH];
#endif

    if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath)))
        return "";

    char snfile_path[MAX_PATH];

#ifndef UNICODE

    size_t newsize = strlen(szPath) + 1;
    wchar_t * wcstring = new wchar_t[newsize];
    // Convert char* string to a wchar_t* string.
    size_t convertedChars = 0;
    mbstowcs_s(&convertedChars, wcstring, newsize, szPath, _TRUNCATE);
    wcstombs(snfile_path, wcstring, MAX_PATH);
#else
    wcstombs(snfile_path, szPath, MAX_PATH);
#endif

    std::string filename(snfile_path);
    filename += "\\Stereolabs\\";

#else //LINUX
    std::string homepath = getenv("HOME");
    std::string filename = homepath + "/zed/";
#endif

    return filename;
}

/*return the path to the Sl ZED hidden dir*/
static inline std::string getHiddenDir() {
    std::string filename = getRootHiddenDir();
#ifdef WIN32
    filename += "settings\\";
#else //LINUX
    filename += "settings/";
#endif
    return filename;
}

bool downloadCalibrationFile(unsigned int serial_number, std::string &calibration_file) {
#ifndef _WIN32
    std::string path = getHiddenDir();
    char specific_name[128];
    sprintf(specific_name, "SN%d.conf", serial_number);
    calibration_file = path + specific_name;
    if (!checkFile(calibration_file)) {
        std::string cmd;
        int res;

        // Create download folder
        cmd = "mkdir -p " + path;
        res = system(cmd.c_str());

        // Download the file
        std::string url("'https://calib.stereolabs.com/?SN=");

        cmd = "wget " + url + std::to_string(serial_number) + "' -O " + calibration_file;
        std::cout << cmd << std::endl;
        res = system(cmd.c_str());

        if( res == EXIT_FAILURE )
        {
            std::cerr << "Error downloading the calibration file" << std::endl;
            return false;
        }

        if (!checkFile(calibration_file)) {
            std::cerr << "Invalid calibration file" << std::endl;
            return false;
        }
    }
#else
    std::string path = getHiddenDir();
    char specific_name[128];
    sprintf(specific_name, "SN%d.conf", serial_number);
    calibration_file = path + specific_name;
    if (!checkFile(calibration_file)) {
        TCHAR *settingFolder = new TCHAR[path.size() + 1];
        settingFolder[path.size()] = 0;
        std::copy(path.begin(), path.end(), settingFolder);
        SHCreateDirectoryEx(NULL, settingFolder, NULL); //recursive creation

        std::string url("https://calib.stereolabs.com/?SN=");
        url += std::to_string(serial_number);
        TCHAR *address = new TCHAR[url.size() + 1];
        address[url.size()] = 0;
        std::copy(url.begin(), url.end(), address);
        TCHAR *calibPath = new TCHAR[calibration_file.size() + 1];
        calibPath[calibration_file.size()] = 0;
        std::copy(calibration_file.begin(), calibration_file.end(), calibPath);

        HRESULT hr = URLDownloadToFile(NULL, address, calibPath, 0, NULL);
        if (hr != 0) {
            std::cout << "Fail to download calibration file" << std::endl;
            return false;
        }

        if (!checkFile(calibration_file)) {
            std::cout << "Invalid calibration file" << std::endl;
            return false;
        }
    }
#endif

    return true;
}

// OpenCV includes
#include <opencv2/opencv.hpp>

bool initCalibration(std::string calibration_file, cv::Size2i image_size, cv::Mat &map_left_x, cv::Mat &map_left_y,
        cv::Mat &map_right_x, cv::Mat &map_right_y, cv::Mat &cameraMatrix_left, cv::Mat &cameraMatrix_right, double *baseline=nullptr) {

    if (!checkFile(calibration_file)) {
        std::cout << "Calibration file missing." << std::endl;
        return 0;
    }

    // Open camera configuration file
    ConfManager camerareader(calibration_file.c_str());
    if (!camerareader.isOpened())
        return 0;

    std::string resolution_str;
    switch ((int) image_size.width) {
        case 2208:
            resolution_str = "2k";
            break;
        case 1920:
            resolution_str = "fhd";
            break;
        case 1280:
            resolution_str = "hd";
            break;
        case 672:
            resolution_str = "vga";
            break;
        default:
            resolution_str = "hd";
            break;
    }

    // Get translations
    float T_[3];
    T_[0] = camerareader.getValue("stereo:baseline", 0.0f);
    T_[1] = camerareader.getValue("stereo:ty_" + resolution_str, 0.f);
    T_[2] = camerareader.getValue("stereo:tz_" + resolution_str, 0.f);

    if(baseline) *baseline=T_[0];

    // Get left parameters
    float left_cam_cx = camerareader.getValue("left_cam_" + resolution_str + ":cx", 0.0f);
    float left_cam_cy = camerareader.getValue("left_cam_" + resolution_str + ":cy", 0.0f);
    float left_cam_fx = camerareader.getValue("left_cam_" + resolution_str + ":fx", 0.0f);
    float left_cam_fy = camerareader.getValue("left_cam_" + resolution_str + ":fy", 0.0f);
    float left_cam_k1 = camerareader.getValue("left_cam_" + resolution_str + ":k1", 0.0f);
    float left_cam_k2 = camerareader.getValue("left_cam_" + resolution_str + ":k2", 0.0f);
    float left_cam_p1 = camerareader.getValue("left_cam_" + resolution_str + ":p1", 0.0f);
    float left_cam_p2 = camerareader.getValue("left_cam_" + resolution_str + ":p2", 0.0f);
    float left_cam_k3 = camerareader.getValue("left_cam_" + resolution_str + ":k3", 0.0f);

    // Get right parameters
    float right_cam_cx = camerareader.getValue("right_cam_" + resolution_str + ":cx", 0.0f);
    float right_cam_cy = camerareader.getValue("right_cam_" + resolution_str + ":cy", 0.0f);
    float right_cam_fx = camerareader.getValue("right_cam_" + resolution_str + ":fx", 0.0f);
    float right_cam_fy = camerareader.getValue("right_cam_" + resolution_str + ":fy", 0.0f);
    float right_cam_k1 = camerareader.getValue("right_cam_" + resolution_str + ":k1", 0.0f);
    float right_cam_k2 = camerareader.getValue("right_cam_" + resolution_str + ":k2", 0.0f);
    float right_cam_p1 = camerareader.getValue("right_cam_" + resolution_str + ":p1", 0.0f);
    float right_cam_p2 = camerareader.getValue("right_cam_" + resolution_str + ":p2", 0.0f);
    float right_cam_k3 = camerareader.getValue("right_cam_" + resolution_str + ":k3", 0.0f);

    // (Linux only) Safety check A: Wrong "." or "," reading in file conf.
#ifndef _WIN32
    if (right_cam_k1 == 0 && left_cam_k1 == 0 && left_cam_k2 == 0 && right_cam_k2 == 0) {
        std::cout << "ZED File invalid" << std::endl;

        std::string cmd = "rm " + calibration_file;
        int res = system(cmd.c_str());
        if( res == EXIT_FAILURE )
        {
            exit(1);
        }

        exit(1);
    }
#endif

    // Get rotations
    cv::Mat R_zed = (cv::Mat_<double>(1, 3) << camerareader.getValue("stereo:rx_" + resolution_str, 0.f), camerareader.getValue("stereo:cv_" + resolution_str, 0.f), camerareader.getValue("stereo:rz_" + resolution_str, 0.f));
    cv::Mat R;

    cv::Rodrigues(R_zed /*in*/, R /*out*/);

    cv::Mat distCoeffs_left, distCoeffs_right;

    // Left
    cameraMatrix_left = (cv::Mat_<double>(3, 3) << left_cam_fx, 0, left_cam_cx, 0, left_cam_fy, left_cam_cy, 0, 0, 1);
    distCoeffs_left = (cv::Mat_<double>(5, 1) << left_cam_k1, left_cam_k2, left_cam_p1, left_cam_p2, left_cam_k3);

    // Right
    cameraMatrix_right = (cv::Mat_<double>(3, 3) << right_cam_fx, 0, right_cam_cx, 0, right_cam_fy, right_cam_cy, 0, 0, 1);
    distCoeffs_right = (cv::Mat_<double>(5, 1) << right_cam_k1, right_cam_k2, right_cam_p1, right_cam_p2, right_cam_k3);

    // Stereo
    cv::Mat T = (cv::Mat_<double>(3, 1) << T_[0], T_[1], T_[2]);
    std::cout << " Camera Matrix L: \n" << cameraMatrix_left << std::endl << std::endl;
    std::cout << " Camera Matrix R: \n" << cameraMatrix_right << std::endl << std::endl;

    cv::Mat R1, R2, P1, P2, Q;
    cv::stereoRectify(cameraMatrix_left, distCoeffs_left, cameraMatrix_right, distCoeffs_right, image_size, R, T,
            R1, R2, P1, P2, Q, cv::CALIB_ZERO_DISPARITY, 0, image_size);

    //Precompute maps for cv::remap()
    initUndistortRectifyMap(cameraMatrix_left, distCoeffs_left, R1, P1, image_size, CV_32FC1, map_left_x, map_left_y);
    initUndistortRectifyMap(cameraMatrix_right, distCoeffs_right, R2, P2, image_size, CV_32FC1, map_right_x, map_right_y);

    cameraMatrix_left = P1;
    cameraMatrix_right = P2;

    return 1;
}

} // namespace oc_tools
} // namespace sl_oc


#endif // CONF_MANAGER_HPP
