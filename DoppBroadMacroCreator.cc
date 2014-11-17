using namespace std;

#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <dirent.h>
#include "include/ElementNames.hh"
#include <iomanip>

// add header file to the original string stream
// use findDouble() when determining if the constructor is a single isotope or not

enum  OutFilter {characters=1, numbers, NA, symbols};

void GetDataStream( string, std::stringstream&);

void FormatData(std::stringstream& streamS, std::stringstream& streamH);
bool MovePastWord(std::stringstream& stream, string word);
string ExtractString(std::stringstream &stream, char delim, int outType=7);
void CropStream(std::stringstream& stream, int firstChar, int lastChar=0);
void FindMaterialList(std::stringstream& stream, std::vector<string> &matNameList);
void GetIsotopeList(std::stringstream& stream, std::vector<string> &matNameList, std::vector<string> &isoNameList, std::vector<double> &isoTempVec, std::stringstream &original);
bool FindConstructor(std::stringstream& stream, string name, std::vector<string> &isoNameList, std::vector<double> &isoTempVec, string matType, double matTemp=0, std::stringstream *original=NULL, bool matSet=false);
double FindMatTemp(std::stringstream& stream, string matName, bool normal, std::stringstream *original=NULL);
int FindElementList(std::stringstream& stream, string matName, std::vector<string> &matNameList, std::vector<string> &elemNameList, std::vector<string> &isoNameList, std::vector<double> &isoTempVec, double matTemp);
void FindIsotopeList(std::stringstream& stream, string elemName, std::vector<string> &elemNameList, std::vector<string> &isoNameList, std::vector<double> &isoTempVec, double matTemp);
bool findDouble(std::stringstream *stream, string variable, double &temperature);
void GetAndAddIsotope(std::stringstream& stream, std::vector<string> &isoNameList, std::vector<double> &isoTempVec, double matTemp);

string CreateMacroName(string geoFileName, string outDirName);
void SetDataStream( string, std::stringstream&);



int main(int argc, char **argv)
{
    string geoFileSourceName, geoFileHeaderName, outDirName;
    ElementNames elementNames;
    elementNames.SetElementNames();
    std::stringstream streamS, streamH;
    string macroFileName;
    if(argc>=4&&(floor(argc/2)==ceil(argc/2)))
    {
        outDirName = argv[1];
        for(int i = 2; i<argc; i+=2)
        {
            geoFileSourceName = argv[i];
            geoFileHeaderName = argv[i+1];
            GetDataStream(geoFileSourceName, streamS);
            GetDataStream(geoFileHeaderName, streamH);
            FormatData(streamS, streamH);
            macroFileName = CreateMacroName(geoFileSourceName, outDirName);
            SetDataStream( macroFileName, streamS);
        }

        cout << "\nMacro file creation is complete, don't forget to fill in the DoppBroad run parameters at the top of the macrofile before using it\n" << endl;
    }
    else
    {
        cout << "\nGive the the output directory and then the name of the source and the header file (in that order) for each G4Stork geometry that you want to convert\n" <<  endl;
    }

    elementNames.ClearStore();
}

void GetDataStream( string geoFileName, std::stringstream& ss)
{
    string* data=NULL;

    // Use regular text file
    std::ifstream thefData( geoFileName.c_str() , std::ios::in | std::ios::ate );
    if ( thefData.good() )
    {
        int file_size = thefData.tellg();
        thefData.seekg( 0 , std::ios::beg );
        char* filedata = new char[ file_size ];
        while ( thefData )
        {
            thefData.read( filedata , file_size );
        }
        thefData.close();
        data = new string ( filedata , file_size );
        delete [] filedata;
    }
    else
    {
    // found no data file
    //                 set error bit to the stream
        ss.setstate( std::ios::badbit );
    }
    if (data != NULL)
    {
        ss.str(*data);
        if(data->back()!='\n')
            ss << "\n";
        ss.seekg( 0 , std::ios::beg );
    }

    delete data;
}

void FormatData(std::stringstream& stream, std::stringstream& stream2)
{
    std::vector<string> matNameList;
    std::vector<string> isoNameList;
    std::vector<double> isoTempVec;
    std::stringstream original;

    original.str(stream2.str()+stream.str());

    MovePastWord(stream, "::ConstructMaterials()");
    int pos = stream.tellg();
    CropStream(stream, pos);
    FindMaterialList(stream, matNameList);
    GetIsotopeList(stream, matNameList, isoNameList, isoTempVec, original);

    stream.str("");
    stream.clear();

    stream << "(int: # of parameters)\n" << "(string: CS data input file or directory)\n" << "(string: CS data output file or directory)\n"
            << "(bool: use the file in the input directory with the closest temperature)\n" << "(double: use the file in the input directory with this temperature)\n"
            << "[Optional](string: choose either ascii or compressed for the output file type {Default=ascii})\n" << "[Optional](bool: create log file to show progress and errors {Default=false})\n"
            << "[Optional](bool: regenerate any existing doppler broadened data file with the same name {Default=true})\n" << isoNameList.size() << "\n\n"
            << "Fill in the above parameters and then delete this line before running.\n" << "The order of the parameters must be mantianed,\n"
            << "to enter an option the user must enter the previous options on the list \nleave the number at the bottom this is your # of isotopes\n\n";

    for(int i=0; i<int(isoNameList.size()); i++)
    {
        stream.fill(' ');
        stream << std::setw(20) << std::left << isoNameList[i] << std::setw(14) << std::left << isoTempVec[i] << '\n';
    }
}

bool MovePastWord(std::stringstream& stream, string word)
{
    std::vector<string> wordParts;
    int pos=0, start;
    bool check=true, firstPass=true;

    start = stream.tellg();

    for(int i=0; i<int(word.length()); i++)
    {
        if(word[i]==' ')
        {
            if(check)
            {
                pos=i+1;
            }
            else
            {
                wordParts.push_back(word.substr(pos,i-pos));
                pos=i+1;
                check=true;
            }
        }
        else
        {
            check=false;
            if(i==int(word.length()-1))
            {
                wordParts.push_back(word.substr(pos,i-pos+1));
            }
        }
    }

    if(wordParts.size()==0)
    {
        wordParts.push_back(word);
    }

    string wholeWord, partWord;
    check=false;
    char line[256];

    while(!check)
    {
        if(!stream)
        {
            if(firstPass)
            {
                stream.clear();
                stream.seekg(start, std::ios::beg);
                firstPass=false;
            }
            else
            {
                break;
            }
        }
        if(stream.peek()=='/')
        {
            stream.get();
            if(stream.peek()=='/')
            {
                stream.getline(line,256);
            }
            else if(stream.peek()=='*')
            {
                stream.get();
                while(stream)
                {
                    if(stream.get()=='*')
                    {
                        if(stream.get()=='/')
                        {
                            break;
                        }
                    }
                }
            }
        }
        else if(stream.peek()=='\n')
        {
            stream.getline(line,256);
        }
        else if(stream.peek()=='\t')
        {
            stream.get();
        }
        else if(stream.peek()==' ')
        {
            stream.get();
        }
        else
        {
            for(int i=0; i<int(wordParts.size()); i++)
            {
                stream >> wholeWord;
                if(int(wholeWord.length())>=int((wordParts[i]).length()))
                {
                    if(firstPass)
                    {
                        check=(wholeWord==(wordParts[i]));
                        if(!check)
                        {
                            break;
                        }
                    }
                    else
                    {
                        partWord = wholeWord.substr(0, (wordParts[i]).length());
                        check=(partWord==(wordParts[i]));

                        if(check)
                        {
                            stream.seekg((partWord.length()-wholeWord.length()),std::ios_base::cur);
                        }
                        else if(0==i)
                        {
                            partWord = wholeWord.substr(wholeWord.length()-(wordParts[i]).length(), (wordParts[i]).length());
                            check=(partWord==(wordParts[i]));
                        }

                        if(!check)
                        {
                            break;
                        }
                    }

                }
                else
                {
                    break;
                }
            }
        }

    }

    if(!check)
    {
        stream.clear();
        stream.seekg(start, std::ios::beg);
    }

    return check;
}

string ExtractString(std::stringstream &stream, char delim, int outType)
{
    string value="";
    bool charOut=false, numOut=false, symOut=false;
    char letter;
    //bool first=true;

    if(outType==0)
    {

    }
    else if(outType==1)
    {
        charOut=true;
    }
    else if(outType==2)
    {
        numOut=true;
    }
    else if(outType==3)
    {
        charOut=true;
        numOut=true;
    }
    else if(outType==4)
    {
        symOut=true;
    }
    else if(outType==5)
    {
        charOut=true;
        symOut=true;
    }
    else if(outType==6)
    {
        numOut=true;
        symOut=true;
    }
    else
    {
        charOut=true;
        numOut=true;
        symOut=true;
    }

    while(stream&&(stream.peek()!=delim))
    {
        letter = stream.get();
        if(((letter>='A')&&(letter<='Z'))||((letter>='a')&&(letter<='z')))
        {
            if(charOut)
            {
                value+=letter;
                //first=true;
            }
            /*else if(first)
            {
                value+=' ';
                first=false;
            }*/
        }
        else if(((letter>='0')&&(letter<='9'))||(letter=='.')||(letter=='-'))
        {
            if(numOut)
            {
                value+=letter;
                //first=true;
            }
            /*else if(first)
            {
                value+=' ';
                first=false;
            }*/
        }
        else
        {
            if(symOut)
            {
                value+=letter;
                //first=true;
            }
            /*else if(first)
            {
                value+=' ';
                first=false;
            }*/
        }
    }
    return value;
}

void CropStream(std::stringstream& stream, int firstChar, int lastChar)
{
    if(lastChar==0)
        stream.seekg( 0 , std::ios::end );
    else
        stream.seekg( lastChar , std::ios::beg );

    int file_size = int(stream.tellg())-firstChar;
    stream.seekg( firstChar , std::ios::beg );
    char* filedata = new char[ file_size ];

    stream.read( filedata , file_size );
    if(!stream)
    {
        cout << "\n #### Error reading string stream ###" << endl;
        return;
    }
    stream.str("");
    stream.clear();

    stream.write( filedata, file_size);
    stream.seekg(0 , std::ios::beg);

    delete filedata;
}

void FindMaterialList(std::stringstream& stream, std::vector<string> &matNameList)
{
    string name="";
    while(MovePastWord(stream, "matMap["))
    {
        stream.get();

        ExtractString(stream, '=', 0);

        stream.get();

        name=ExtractString(stream, ';', int(characters+numbers));
        if(name!="")
        {
            matNameList.push_back(name);
        }
        else
        {
            cout << "\nError: found a blank when trying to extract material name\n" << endl;
        }
        name.clear();

    }
    stream.clear();
    stream.seekg(0, std::ios::beg);
}

void GetIsotopeList(std::stringstream& stream, std::vector<string> &matNameList, std::vector<string> &isoNameList, std::vector<double> &isoTempVec, std::stringstream &original)
{
    std::vector<string> elemNameList;
    std::vector<double> tempList;
    double matTemp;
    int initialSize = matNameList.size(), addMat;
    bool matSet=false;

    for(int i=0; i<int(matNameList.size()); i++)
    {
        if(i>initialSize-1)
        {
            matSet=true;
            matTemp=tempList[i-initialSize];
        }
        if(FindConstructor(stream, matNameList[i], isoNameList, isoTempVec, "Material", matTemp, &original, matSet))
        {
            if(!(i>initialSize-1))
            {
                matTemp=FindMatTemp(stream, matNameList[i], true, &original );
            }

            addMat=FindElementList(stream, matNameList[i], matNameList, elemNameList, isoNameList, isoTempVec, matTemp);
            while(addMat>0)
            {
                tempList.push_back(matTemp);
                addMat--;
            }

            for(int j=0; j<int(elemNameList.size()); j++)
            {
                FindIsotopeList(stream, elemNameList[j], elemNameList, isoNameList, isoTempVec, matTemp);
            }
            elemNameList.clear();

        }
        stream.clear();
        stream.seekg(0, std::ios::beg);

    }
}

bool FindConstructor(std::stringstream& stream, string name, std::vector<string> &isoNameList, std::vector<double> &isoTempVec, string matType,
                    double matTemp, std::stringstream *original, bool matSet)
{
    string check;
    std::stringstream line;
    int pos;
    if(!MovePastWord(stream, (name+" =")))
    {
        cout << "\nError: could not find constructor for " << name << "\n" << endl;
        return false;
    }
    else
    {
        pos=stream.tellg();
        int count=0;

        if(matType=="Material")
        {
            bool intType=true;
            int pos1=pos;
            while(stream.peek()!=')')
            {
                if(stream.get()==',')
                {
                    if(count==0)
                    {
                        pos1=stream.tellg();
                    }
                    count++;
                }
                if(count==3)
                {
                    line.str(ExtractString(stream, ')', int(characters+numbers+symbols))+")");
                    line.seekg(0,std::ios::beg);
                    bool test=false;
                    while((line.peek()!=')')&&(line.peek()!=','))
                    {
                        if(line.get()=='.')
                        {
                            intType=false;
                            break;
                        }
                        if(line.peek()==',')
                        {
                            test=true;
                        }
                    }
                    if(intType)
                    {
                        line.seekg(0,std::ios::beg);
                        string variable;
                        double num;
                        if(test)
                        {
                            variable = ExtractString(line, ',', int(characters+numbers));
                        }
                        else
                        {
                            variable = ExtractString(line, ')', int(characters+numbers));
                        }
                        if (findDouble(original, variable, num))
                        {
                            stringstream numConv;
                            numConv << num;
                            while(numConv)
                            {
                                if(numConv.get()=='.')
                                {
                                    intType=false;
                                    break;
                                }
                            }
                        }
                    }
                    if(intType)
                    {
                        intType = (!MovePastWord(line, "kState"));
                    }
                    line.str("");
                    line.clear();
                    break;
                }
            }
            if(intType)
            {
                stream.seekg(pos1, std::ios::beg);
                return true;
            }
            else
            {
                stream.seekg(pos1, std::ios::beg);
                if(!matSet)
                    matTemp=FindMatTemp(stream, name, false, original);
                stream.seekg(pos1, std::ios::beg);
                GetAndAddIsotope(stream, isoNameList, isoTempVec, matTemp);
                return false;
            }

        }
        else if(matType=="Element")
        {
            int count=0, pos1=0, pos2=0;
            while(stream.peek()!=';')
            {
                if(stream.get()==',')
                {
                    pos1=pos2;
                    pos2=stream.tellg();
                    count++;
                }
            }
            if(count==3)
            {
                stream.seekg(pos1, std::ios::beg);
                GetAndAddIsotope(stream, isoNameList, isoTempVec, matTemp);
                return false;
            }
            else
            {
                stream.seekg(pos, std::ios::beg);
                return true;
            }
        }
        else
        {
            return true;
        }
    }
}

double FindMatTemp(std::stringstream& stream, string matName, bool normal, std::stringstream *original)
{
    int count=0, limit;
    bool celsius=false, standard=true, number=true, first=true;
    double temperature=0.;
    char letter;
    std::stringstream temp;

    if(normal)
    {
        limit=3;
    }
    else
    {
        limit=4;
    }

    while(stream.peek()!=';')
    {
        if((stream.get())==',')
        {
            count++;
        }
        if(limit==count)
        {
            standard=false;
            break;
        }
    }
    if(standard)
    {
        temperature=273.15;
    }
    else
    {
        while((stream.peek()!=',')&&(stream.peek()!=')'))
        {
            letter=stream.get();
            if(((letter>='0')&&(letter<='9'))||(letter=='.')||(letter=='-'))
            {
                if(first)
                {
                    number=true;
                    first=false;
                }
                temp << letter;
            }
            else if((((letter>='A')&&(letter<='Z'))||((letter>='a')&&(letter<='z')))||(letter=='[')||(letter==']')||(letter==','))
            {
                if((letter=='c')||(letter=='C'))
                {
                    celsius=true;
                }
                if(first)
                {
                    number=false;
                    first=false;
                }
                if(!number)
                    temp << letter;
            }
        }

        if(temp.str()!="")
        {
            if(number)
            {
                temp >> temperature;
                if(celsius)
                {
                    temperature+=273.15;
                }
            }
            else if(original!=NULL)
            {
                if(!findDouble(original, temp.str(), temperature))
                {
                    cout << "\nError: couldn't find material temperature " << matName << endl;
                }
            }
        }
        else
        {
            cout << "\nError: unable to find temperature for " << matName << " in the expected position\n" << endl;
        }
    }

    return temperature;
}

bool findDouble(std::stringstream *stream, string variable, double &temperature)
{
    bool arrayElem=false, number=false, celsius=false, first=true;
    std::vector<int> arrayIndex;
    int index, pos1, pos2, count=0;
    stringstream numConv, temp;
    char letter;
    stream->seekg(0, std::ios::beg);

    while(variable.back()==']')
    {
        arrayElem=true;
        pos1=variable.find_first_of('[',0);
        pos2=variable.find_first_of(']',0);
        numConv.str(variable.substr(pos1,pos1-pos2-1));
        numConv >> index;
        numConv.clear();
        numConv.str("");
        arrayIndex.push_back(index);
        variable.erase(pos1, pos2-pos1+1);
    }

    if(variable!="")
    {
        if(*stream)
        {
            if(MovePastWord((*stream),variable+" ="))
            {
                temp.str(ExtractString((*stream),';',int(numbers+characters)));
                temp.str(temp.str()+';');

                if(arrayElem)
                {
                    for(int i=0; i<int(arrayIndex.size()); i++)
                    {
                        ExtractString(temp,'{',0);
                        temp.get();
                        while(count!=arrayIndex[i])
                        {
                            letter=temp.get();
                            if(letter=='{')
                            {
                               ExtractString(temp,'}',0);
                               temp.get();
                            }
                            else if(letter==',')
                            {
                                count++;
                            }
                        }
                    }
                }
                while((temp.peek()!=',')&&(temp.peek()!=';'))
                {
                    letter=temp.get();
                    if(((letter>='0')&&(letter<='9'))||(letter=='.')||(letter=='-'))
                    {
                        if(first)
                        {
                            number=true;
                            first=false;
                        }
                        numConv << letter;
                    }
                    else if((((letter>='A')&&(letter<='Z'))||((letter>='a')&&(letter<='z')))||(letter=='[')||(letter==']')||(letter==','))
                    {
                        if((letter=='c')||(letter=='C'))
                        {
                            celsius=true;
                        }
                        if(first)
                        {
                            number=false;
                            first=false;
                        }
                        if(!number)
                            numConv << letter;
                    }
                }
                if(number)
                {
                    numConv >> temperature;
                    if(celsius)
                    {
                        temperature+=273.15;
                    }
                }
                else
                {
                    (*stream).seekg(0,std::ios::beg);
                    return findDouble(stream, numConv.str(), temperature);
                }

            }
            else
            {
                return false;
            }
        }
    }
    else
    {
        return false;
    }

    return true;
}

int FindElementList(std::stringstream& stream, string matName, std::vector<string> &matNameList, std::vector<string> &elemNameList, std::vector<string> &isoNameList, std::vector<double> &isoTempVec, double matTemp)
{
    string name="";
    int addMat=0;
    std::stringstream checkCon;

    while(MovePastWord(stream, matName+" ->"))
    {
        name.clear();
        name=ExtractString(stream, '(', int(characters));

        stream.get();

        if(name=="AddElement")
        {
            name.clear();
            name=ExtractString(stream, ',', int(characters+numbers));
            stream.get();
            checkCon.clear();
            checkCon.str(name);
            if(MovePastWord(checkCon, "new G4Element"))
            {
                ExtractString(stream, ',', 0);
                GetAndAddIsotope(stream, isoNameList, isoTempVec, matTemp);
            }
            else if(name!="")
            {
                elemNameList.push_back(name);
            }
            else
            {
                cout << "\nError: found a blank when trying to extract element name\n" << endl;
            }
        }
        else if(name=="AddMaterial")
        {
            name.clear();

            name=ExtractString(stream, ',', int(characters+numbers));
            stream.get();
            checkCon.clear();
            checkCon.str(name);
            if(MovePastWord(checkCon, "new G4Material"))
            {
                GetAndAddIsotope(stream, isoNameList, isoTempVec, matTemp);
            }
            else if(name!="")
            {
                matNameList.push_back(name);
                addMat++;
            }
            else
            {
                cout << "\nError: found a blank when trying to extract material name\n" << endl;
            }
        }

    }
    stream.clear();
    stream.seekg(0, std::ios::beg);

    return addMat;
}

void FindIsotopeList(std::stringstream& stream, string elemName, std::vector<string> &elemNameList, std::vector<string> &isoNameList, std::vector<double> &isoTempVec, double matTemp)
{
    std::vector<string> isoObjectNameList;
    std::stringstream checkCon;

    if(FindConstructor(stream, elemName, isoNameList, isoTempVec, "Element", matTemp))
    {
        string name="";
        while(MovePastWord(stream, elemName+" ->"))
        {
            name.clear();
            name=ExtractString(stream, '(', int(characters));

            stream.get();

            if(name=="AddIsotope")
            {
                name.clear();
                name=ExtractString(stream, ',', int(characters+numbers));
                stream.get();
                checkCon.clear();
                checkCon.str(name);
                if(MovePastWord(checkCon, "new G4Isotope"))
                {
                    GetAndAddIsotope(stream, isoNameList, isoTempVec, matTemp);
                }
                else if(name!="")
                {
                    isoObjectNameList.push_back(name);
                }
                else
                {
                    cout << "\nError: found a blank when trying to extract isotope name\n" << endl;
                }
            }
            else if(name=="AddElement")
            {
                name.clear();
                name=ExtractString(stream, ',', int(characters+numbers));
                stream.get();
                checkCon.clear();
                checkCon.str(name);
                if(MovePastWord(checkCon, "new G4Element"))
                {
                    ExtractString(stream, ',', 0);
                    GetAndAddIsotope(stream, isoNameList, isoTempVec, matTemp);
                }
                else if(name!="")
                {
                    elemNameList.push_back(name);
                }
                else
                {
                    cout << "\nError: found a blank when trying to extract element name\n" << endl;
                }
            }
        }
        stream.clear();
        stream.seekg(0, std::ios::beg);
    }

    for(int i=0; i<int(isoObjectNameList.size()); i++)
    {
        if(FindConstructor(stream, isoObjectNameList[i], isoNameList, isoTempVec, "Isotope"))
        {
            ExtractString(stream, ',', 0);
            stream.get();

            GetAndAddIsotope(stream, isoNameList, isoTempVec, matTemp);
        }
        else
        {
            cout << "\nError: couldn't fin isotope constructor for " << isoObjectNameList[i] << endl;
        }
        //I changed this check and make sure it still works
        stream.clear();
        stream.seekg(0, std::ios::beg);
    }
}

void GetAndAddIsotope(std::stringstream& stream, std::vector<string> &isoNameList, std::vector<double> &isoTempVec, double matTemp)
{
    std::stringstream isoName;
    ElementNames* elementNames;
    bool duplicate;
    int Z;

    isoName << ExtractString(stream, ',', int(numbers));

    stream.get();
    isoName >> Z;
    isoName.clear();
    isoName << '_';

    isoName << (ExtractString(stream, ',', int(numbers))).c_str();

    isoName << "_" << elementNames->GetName(Z);
    duplicate=false;
    for(int j=0; j<int(isoNameList.size()); j++)
    {
        if(isoNameList[j]==isoName.str())
        {
            if(isoTempVec[j]==matTemp)
            {
                duplicate=true;
            }
        }
    }
    if(!duplicate)
    {
        isoNameList.push_back(isoName.str());
        isoTempVec.push_back(matTemp);
    }
    isoName.str("");
    isoName.clear();
}

string CreateMacroName(string geoFileName, string outDirName)
{
    if((geoFileName.substr(geoFileName.length()-3,3))==".cc")
    {
        geoFileName=geoFileName.substr(0,geoFileName.length()-3);
    }
    size_t pos = geoFileName.find_last_of('/');
    size_t pos2 = std::string::npos;
    if(pos == std::string::npos)
        pos=0;
    else
        pos++;

    if(geoFileName.length()>11)
    {
        string test = geoFileName.substr(geoFileName.length()-11, 11);
        if((test=="Constructor")||(test=="constructor"))
        {
            pos2 = geoFileName.length()-11;
        }
    }

    return (outDirName+"DoppBroadMacro"+geoFileName.substr(pos, pos2-pos)+".txt");
}

void SetDataStream( string macroFileName, std::stringstream& ss)
{
  std::ofstream out( macroFileName.c_str() , std::ios::out | std::ios::trunc );
  if ( ss.good() )
  {
     ss.seekg( 0 , std::ios::end );
     int file_size = ss.tellg();
     ss.seekg( 0 , std::ios::beg );
     char* filedata = new char[ file_size ];

     while ( ss )
     {
        ss.read( filedata , file_size );
        if(!file_size)
        {
            cout << "\n #### Error the size of the stringstream is invalid ###" << endl;
            break;
        }
     }

     out.write(filedata, file_size);
     if (out.fail())
    {
        cout << endl << "writing the ascii data to the output file " << macroFileName << " failed" << endl
             << " may not have permission to delete an older version of the file" << endl;
    }
     out.close();
     delete [] filedata;
  }
  else
  {
// found no data file
//                 set error bit to the stream
     ss.setstate( std::ios::badbit );

     cout << endl << "### failed to write to ascii file " << macroFileName << " ###" << endl;
  }
   ss.str("");
}
