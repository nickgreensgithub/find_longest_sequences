#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <utility>
#include <vector>
#include <chrono>
#include <iterator>
#include <map>
#include "include/progressbar.h"
#include <set>

using namespace std;

struct fastaEntry{
    string header;
    string sequence;
    int sequenceLength;
    bool eliminated = false;

    fastaEntry(const string& header, const string& sequence){
        this->header = std::move(header);
        this->sequence = sequence;
        sequenceLength = sequence.length();
    }

    operator const string(){
        ostringstream fastaStream;
        fastaStream << header << endl << sequenceLength << endl;
        return fastaStream.str();
    }

    bool IsShorterVersionOf (const fastaEntry *otherFastaEntry) const {
        if (otherFastaEntry->sequence.find(this->sequence) != string::npos) {
            if(otherFastaEntry->sequenceLength > this->sequenceLength){
                return true;
            }
        }
        return false;
    }
};

int THREADS;
vector<fastaEntry*> SORTED_RECORDS;
map<int,int> SEARCH_INDEX;

vector<fastaEntry> ReadFromFastaFile(string path);
void WriteToFastaFile(string path, vector<fastaEntry*> &records);
vector<fastaEntry*> filterRecords(vector<fastaEntry> &unsortedRecords, vector<fastaEntry*> &sortedRecords);
vector<vector<fastaEntry*>> SplitVectorIntoChunks(vector<fastaEntry>& records, int n = THREADS);
map<int, int>  CreateSequenceLengthSearchIndex(vector<fastaEntry*> sortedRecords);
int GetSearchStartingIndex(const fastaEntry& thisRecord);
int CheckArgumentCount(int argc);
vector<fastaEntry*> GetChunk(vector<fastaEntry> &items, int startIndex, long count);
vector<fastaEntry*> recombineChunks(const vector<vector<fastaEntry*>> &chunks);

bool sortBySequenceLength (const fastaEntry *i, const fastaEntry *j) {
    return (i->sequenceLength < j->sequenceLength);
}

map<int, int> CreateSequenceLengthSearchIndex(vector<fastaEntry*> sortedRecords){
    map<int, int> lengths{};
    for (auto i = 0; i < sortedRecords.size(); i++) {
        lengths.try_emplace(sortedRecords[i]->sequenceLength, i);
    }
    return lengths;
}

int main(int argc, char* argv[])
{
    auto countStatus = CheckArgumentCount(argc);
    if(countStatus > 0){
        return countStatus;
    }
    if(argc >= 4){
        int threads = stoi(argv[3]);
        THREADS = (threads > 0) ? threads : 1;
    }

    auto inputPath = argv[1];
    auto outputPath = argv[2];

    auto start = chrono::high_resolution_clock::now();
    cout << "Reading input file..." << endl;
    auto records = ReadFromFastaFile(inputPath);
    cout << "done" << endl;
    auto initialRecordNum = records.size();
    auto startPos = records.begin();
    SORTED_RECORDS = GetChunk(records,0, (long) initialRecordNum);

    cout << "Sorting records by sequence length for searching faster..." << endl;
    sort(SORTED_RECORDS.begin(), SORTED_RECORDS.end(), sortBySequenceLength);
    cout << "done" << endl;

    cout << "Creating sequence length index..." << endl;
    SEARCH_INDEX = CreateSequenceLengthSearchIndex(SORTED_RECORDS);
    cout << "done" << endl;


    cout << "Dereplicating..." << endl;
    //TODO multithreading?
    for(auto &seqIndexItem: SEARCH_INDEX) {
        set<string> foundSequences = {};
        auto sizeRange = find_if(SORTED_RECORDS.begin()+seqIndexItem.second, SORTED_RECORDS.end(),[&seqIndexItem](fastaEntry* &thisItem) {
            return thisItem->sequenceLength > seqIndexItem.first;
        });

        for(auto itr = SORTED_RECORDS.begin()+seqIndexItem.second; itr != sizeRange && itr != SORTED_RECORDS.end(); itr++){
            auto currentRecord = **&itr;
            if(foundSequences.contains(currentRecord->sequence)){
                currentRecord->eliminated = true;
            }else{
                foundSequences.insert(currentRecord->sequence);
            }
        }
    }
    cout << "done" << endl;

    cout << "Original record count: " << initialRecordNum << endl;
    auto filtered = filterRecords(records, SORTED_RECORDS);
    cout << endl << "Final record count: "<< filtered.size() << endl;

    cout << "Writing output file" << endl;
    WriteToFastaFile(outputPath, filtered);
    cout << "done" << endl;

    auto stop = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::seconds>(stop - start);
    cout << "Process took: " << duration.count() << " seconds to complete" <<endl;

    return 0;
}

int CheckArgumentCount(int argc){
    int result = 0;
    if (argc < 2) {
        std::cerr << "missing input file" << endl;
        result =  EXIT_FAILURE;
    }
    if (argc < 3) {
        std::cerr << "missing output file" << endl;
        result =  EXIT_FAILURE;
    }
    if(result > 0){
        std::cerr << "usage: longest_sequence <input_file> <output_file> <threads>";
    }
    return result;
}

int GetSearchStartingIndex(const fastaEntry *thisRecord){
    return SEARCH_INDEX[thisRecord->sequenceLength];
}

static bool DoesLongerSequenceExist(const fastaEntry *thisRecord, const vector<fastaEntry*>& records){//records are sorted by length
    auto startIndex = GetSearchStartingIndex(thisRecord);

    return any_of((records.begin() + startIndex), records.end(),[&thisRecord](const fastaEntry *item){
        if(!item->eliminated){
            return thisRecord->IsShorterVersionOf(item);
        }
        return false;
    });
}

vector<fastaEntry*> GetChunk(vector<fastaEntry> &items, int startIndex, long count){
    auto endIndex =  startIndex + count;
    vector<fastaEntry*> result;
    for (int i = startIndex; i < endIndex ;i++){
        auto itemRef = &items.at(i);
        result.push_back(itemRef);
    }
    return result;
}

vector<vector<fastaEntry*>> SplitVectorIntoChunks(vector<fastaEntry>& records, int n) {
    vector<vector<fastaEntry*>> chunks{};
    auto fullSize = records.size();
    int position = 0;
    for (int k = 0; k < n; ++k) {
        long chunkSize = fullSize / (n - k);
        fullSize -= chunkSize;
        chunks.emplace_back(GetChunk(records, position, chunkSize));
        position += chunkSize;
    }
    return chunks;
}

vector<fastaEntry*> filterRecords(vector<fastaEntry> &unsortedRecords, vector<fastaEntry*> &sortedRecords){
    auto chunkedInput = SplitVectorIntoChunks(unsortedRecords);

    //Make progress bar
    progressbar bar(unsortedRecords.size());
    bar.set_todo_char(" ");
    bar.set_done_char("█");
    bar.set_opening_bracket_char("{");
    bar.set_closing_bracket_char("}");

    #pragma omp parallel for num_threads(THREADS)
    for(auto &chunk : chunkedInput) {
        chunk.erase(remove_if(chunk.begin(), chunk.end(), [&sortedRecords, &bar](const fastaEntry *record)->bool{

            //Update the progress bar
            #pragma omp critical
            bar.update();
            if(record->eliminated){
                return true;
            }
            return DoesLongerSequenceExist(record, sortedRecords);
        }),chunk.end());
    }

    return recombineChunks(chunkedInput);
}

vector<fastaEntry*> recombineChunks(const vector<vector<fastaEntry*>> &chunks){
    vector<fastaEntry*> result;
    for(auto &chunk : chunks) {
        for(auto const &record: chunk){
            result.push_back(record);
        }
    }
    return result;
}

vector<fastaEntry> ReadFromFastaFile(string path){
    vector<fastaEntry> fileContent;
    ifstream inputFile;

    inputFile.open(path, ios_base::in);
    if(!inputFile.is_open()){
        cout << "Input file cannot be opened, stopping."<< endl;
        throw "Input error";
    }

    if(inputFile.is_open()){
        string tempLine;
        string tempBuffer;
        while(getline(inputFile, tempLine)){
            if (tempLine.find('>', 0) == 0){
                //Get line until > or end of file
                getline(inputFile, tempBuffer, '>');
                tempBuffer.erase(std::remove(tempBuffer.begin(), tempBuffer.end(), '\n'), tempBuffer.end());
                fastaEntry newRecord = {
                        tempLine,
                        tempBuffer
                };
                fileContent.push_back(newRecord);
                if(!inputFile.eof()){
                    inputFile.putback('>');
                }
            }
        }
        inputFile.close();
    }
    return fileContent;
}

void WriteToFastaFile(string path, vector<fastaEntry*>& records){
    ofstream outputFile;
    outputFile.open(path, ios_base::out | ios_base::trunc);
    if(outputFile.is_open()){
        for(auto & record : records) {
            outputFile << record->header << endl;
            //outputFile << ">FLASV" << i+1 << '.' << records[i].sequenceLength << endl; // Custom header
            outputFile << record->sequence << endl;
        }
    }
}