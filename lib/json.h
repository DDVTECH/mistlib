/// \file json.h Holds all JSON-related headers.

#pragma once
#include <string>
#include <deque>
#include <map>
#include <istream>
#include <vector>
#include <functional>
#include "socket.h"

//empty definition of DTSC::Stream so it can be a friend.
namespace DTSC {
  class Stream;
}

/// JSON-related classes and functions
namespace JSON {

  /// Lists all types of JSON::Value.
  enum ValueType {
    EMPTY, BOOL, INTEGER, STRING, ARRAY, OBJECT
  };

  /// A JSON::Value is either a string or an integer, but may also be an object, array or null.
  class Value {
    private:
      ValueType myType;
      long long int intVal;
      std::string strVal;
      std::deque<Value*> arrVal;
      std::map<std::string, Value*> objVal;
    public:
      //friends
      friend class DTSC::Stream; //for access to strVal
      //constructors/destructors
      Value();
      ~Value();
      Value(const Value & rhs);
      Value(std::istream & fromstream);
      Value(const std::string & val);
      Value(const char * val);
      Value(long long int val);
      Value(bool val);
      //comparison operators
      bool operator==(const Value & rhs) const;
      bool operator!=(const Value & rhs) const;
      //assignment operators
      Value & operator=(const Value & rhs);
      Value & operator=(const std::string & rhs);
      Value & operator=(const char * rhs);
      Value & operator=(const long long int & rhs);
      Value & operator=(const int & rhs);
      Value & operator=(const unsigned int & rhs);
      Value & operator=(const bool & rhs);
      //converts to basic types
      operator long long int() const;
      operator std::string() const;
      operator bool() const;
      const std::string asString() const;
      const long long int asInt() const;
      const bool asBool() const;
      const std::string & asStringRef() const;
      const char * c_str() const;
      //array operator for maps and arrays
      Value & operator[](const std::string i);
      Value & operator[](const char * i);
      Value & operator[](unsigned int i);
      const Value & operator[](const std::string i) const;
      const Value & operator[](const char * i) const;
      const Value & operator[](unsigned int i) const;
      //handy functions and others
      std::string toPacked() const;
      void sendTo(Socket::Connection & socket) const;
      unsigned int packedSize() const;
      void netPrepare();
      std::string & toNetPacked();
      std::string toString() const;
      std::string toPrettyString(int indentation = 0) const;
      void append(const Value & rhs);
      void prepend(const Value & rhs);
      void shrink(unsigned int size);
      void removeMember(const std::string & name);
      bool isMember(const std::string & name) const;
      bool isInt() const;
      bool isString() const;
      bool isBool() const;
      bool isObject() const;
      bool isArray() const;
      bool isNull() const;
      unsigned int size() const;
      void null();
      void forEach(std::function<bool (const Value&)> func) const;
      void forEach(std::function<bool (Value&)> func);
      void forEachMember(std::function<bool (const std::string&, const Value&)> func) const;
      void forEachMember(std::function<bool (const std::string&, Value&)> func);
      void forEachIndice(std::function<bool (const unsigned int, const Value&)> func) const;
      void forEachIndice(std::function<bool (const unsigned int, Value&)> func);
  };

  Value fromDTMI2(std::string & data);
  Value fromDTMI2(const unsigned char * data, unsigned int len, unsigned int & i);
  Value fromDTMI(std::string & data);
  Value fromDTMI(const unsigned char * data, unsigned int len, unsigned int & i);
  Value fromString(std::string json);
  Value fromFile(std::string filename);
  void fromDTMI2(std::string & data, Value & ret);
  void fromDTMI2(const unsigned char * data, unsigned int len, unsigned int & i, Value & ret);
  void fromDTMI(std::string & data, Value & ret);
  void fromDTMI(const unsigned char * data, unsigned int len, unsigned int & i, Value & ret);

  template <typename T>
  std::string encodeVector(T begin, T end) {
    std::string result;
    for (T it = begin; it != end; it++) {
      long long int tmp = (*it);
      while (tmp >= 0xFFFF) {
        result += (char)0xFF;
        result += (char)0xFF;
        tmp -= 0xFFFF;
      }
      result += (char)(tmp / 256);
      result += (char)(tmp % 256);
    }
    return result;
  }

  template <typename T>
  void decodeVector(std::string input, T & result) {
    result.clear();
    unsigned int tmp = 0;
    for (int i = 0; i < input.size(); i += 2) {
      unsigned int curLen = (input[i] << 8) + input[i + 1];
      tmp += curLen;
      if (curLen != 0xFFFF) {
        result.push_back(tmp);
        tmp = 0;
      }
    }
  }

  template <typename T>
  std::string encodeVector4(T begin, T end) {
    std::string result;
    for (T it = begin; it != end; it++) {
      long long int tmp = (*it);
      while (tmp >= 0xFFFFFFFF) {
        result += (char)0xFF;
        result += (char)0xFF;
        result += (char)0xFF;
        result += (char)0xFF;
        tmp -= 0xFFFFFFFF;
      }
      result += (char)((tmp & 0xFF000000) >> 24);
      result += (char)((tmp & 0x00FF0000) >> 16);
      result += (char)((tmp & 0x0000FF00) >> 8);
      result += (char)((tmp & 0x000000FF));
    }
    return result;
  }

  template <typename T>
  void decodeVector4(std::string input, T & result) {
    result.clear();
    unsigned int tmp = 0;
    for (int i = 0; i < input.size(); i += 4) {
      unsigned int curLen = (input[i] << 24) + (input[i + 1] << 16) + (input[i + 2] << 8) + (input[i + 3]);
      tmp += curLen;
      if (curLen != 0xFFFFFFFF) {
        result.push_back(tmp);
        tmp = 0;
      }
    }
  }
}
