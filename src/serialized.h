// serialized.h
//
// Copyright (c) 2019 Kristofer Berggren
// All rights reserved.
//
// nmail is distributed under the MIT license, see LICENSE for details.

#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <vector>

class Serialized
{
public:
  Serialized();
  explicit Serialized(const std::string& p_Path);
  explicit Serialized(const Serialized& p_Serialized);

  void Clear();
  void FromString(const std::string& p_String);
  std::string ToString();
  void Load(const std::string& p_Path);
  void Save(const std::string& p_Path = std::string());

  template <typename T>
  friend Serialized& operator<<(Serialized& p_Serialized, const T& p_Value)
  {
    std::string string;
    FromVal(p_Value, string);
    p_Serialized.m_String += ToHex(string) + "\n";

    return p_Serialized;
  }

  template <typename T>
  friend Serialized& operator<<(Serialized& p_Serialized, const std::vector<T>& p_Vector)
  {
    for (auto& value : p_Vector)
    {
      std::string string;
      FromVal(value, string);
      p_Serialized.m_String += ToHex(string) + " ";
    }
    p_Serialized.m_String += "\n";
    
    return p_Serialized;
  }

  template <typename T>
  friend Serialized& operator<<(Serialized& p_Serialized, const std::set<T>& p_Set)
  {
    for (auto& value : p_Set)
    {
      std::string string;
      FromVal(value, string);
      p_Serialized.m_String += ToHex(string) + " ";
    }
    p_Serialized.m_String += "\n";
    
    return p_Serialized;
  }

  template <typename T, typename U>
  friend Serialized& operator<<(Serialized& p_Serialized, const std::map<T, U>& p_Map)
  {
    for (auto& value : p_Map)
    {
      std::string string;
      FromVal(value.first, string);
      p_Serialized.m_String += ToHex(string) + " ";
      FromVal(value.second, string);
      p_Serialized.m_String += ToHex(string) + " ";
    }
    p_Serialized.m_String += "\n";
    
    return p_Serialized;
  }

  template <typename T>
  friend Serialized& operator>>(Serialized& p_Serialized, T& p_Value)
  {
    std::istringstream iss(p_Serialized.m_String);
    std::string line;
    if (std::getline(iss, line))
    {
      ToVal(FromHex(line), p_Value);
      p_Serialized.m_String = p_Serialized.m_String.substr(line.size() + 1);
    }
        
    return p_Serialized;
  }

  template <typename T>
  friend Serialized& operator>>(Serialized& p_Serialized, std::vector<T>& p_Vector)
  {
    std::istringstream iss(p_Serialized.m_String);
    std::string line;
    if (std::getline(iss, line))
    {
      std::istringstream issl(line);
      std::string word;
      while (issl >> word)
      {
        T value;
        ToVal(FromHex(word), value);
        p_Vector.push_back(value);
      }
      
      p_Serialized.m_String = p_Serialized.m_String.substr(line.size() + 1);
    }
    
    return p_Serialized;
  }

  template <typename T>
  friend Serialized& operator>>(Serialized& p_Serialized, std::set<T>& p_Set)
  {
    std::istringstream iss(p_Serialized.m_String);
    std::string line;
    if (std::getline(iss, line))
    {
      std::istringstream issl(line);
      std::string word;
      while (issl >> word)
      {
        T value;
        ToVal(FromHex(word), value);
        p_Set.insert(value);
      }
      
      p_Serialized.m_String = p_Serialized.m_String.substr(line.size() + 1);
    }
    
    return p_Serialized;
  }
  
  template <typename T, typename U>
  friend Serialized& operator>>(Serialized& p_Serialized, std::map<T, U>& p_Map)
  {
    std::istringstream iss(p_Serialized.m_String);
    std::string line;
    if (std::getline(iss, line))
    {
      std::istringstream issl(line);
      std::string firstWord, secondWord;
      while ((issl >> firstWord) && (issl >> secondWord))
      {
        T firstValue;
        T secondValue;
        ToVal(FromHex(firstWord), firstValue);
        ToVal(FromHex(secondWord), secondValue);
        p_Map.insert(std::pair<T, U>(firstValue, secondValue));
      }
      
      p_Serialized.m_String = p_Serialized.m_String.substr(line.size() + 1);
    }
    
    return p_Serialized;
  }

  static std::string ToHex(const std::string& p_String);

  static std::string FromHex(const std::string& p_String);

private:
  template <typename T>
  static void ToVal(const std::string& p_String, T& p_Value)
  {
    std::istringstream iss(p_String);
    iss >> p_Value;
  }

  template <typename T>
  static void FromVal(const T& p_Value, std::string& p_String)
  {
    std::ostringstream oss;
    oss << p_Value;
    p_String = oss.str();
  }
  
private:
  std::string m_Path;
  std::string m_String;
};


template <>
inline void Serialized::ToVal(const std::string& p_String, std::string& p_Value)
{
  p_Value = p_String;
}

template <>
inline void Serialized::FromVal(const std::string& p_Value, std::string& p_String)
{
  p_String = p_Value;
}


template <typename T>
static void SerializeToFile(const std::string& p_Path, const T& p_Value)
{
  Serialized serialized;
  serialized << p_Value;
  serialized.Save(p_Path);
}

template <typename T>
static void DeserializeFromFile(const std::string& p_Path, T& p_Value)
{
  Serialized serialized;
  serialized.Load(p_Path);
  serialized >> p_Value;
}


template <typename T>
static std::string Serialize(const T& p_Value)
{
  Serialized serialized;
  serialized << p_Value;
  return serialized.ToString();
}

template <typename T>
static T Deserialize(const std::string& p_String)
{
  Serialized serialized;
  serialized.FromString(p_String);
  T value;
  serialized >> value;
  return value;
}
