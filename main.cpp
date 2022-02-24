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

map<char,char> SEQUENCE_COMPARISON_SUBSTITUTIONS {{'U','T'}};

struct fastaSequence{
    string header;
    string originalSequence;
    string substitutedSequence;
    int sequenceLength;
    bool eliminated = false;

    fastaSequence(const string& header, const string& sequence){
        this->header = std::move(header);
        this->originalSequence = sequence;
        sequenceLength = sequence.length();
        CreateSubstitutedSequence();
    }

    operator const string(){
        ostringstream fastaStream;
        fastaStream << header << endl << sequenceLength << endl;
        return fastaStream.str();
    }

    bool IsShorterVersionOf (const fastaSequence *otherFastaEntry) const {
        if (otherFastaEntry->substitutedSequence.find((substitutedSequence)) != string::npos) {
            if(otherFastaEntry->sequenceLength > this->sequenceLength){
                return true;
            }
        }
        return false;
    }

    private: void CreateSubstitutedSequence(){
        substitutedSequence = originalSequence;
        transform(substitutedSequence.begin(), substitutedSequence.end(), substitutedSequence.begin(), ::toupper);
        for (auto substitution: SEQUENCE_COMPARISON_SUBSTITUTIONS) {
            std::replace( substitutedSequence.begin(), substitutedSequence.end(), substitution.first, substitution.second);
        }
    }
};

const string PROGRAM_NAME= "findLongSeqs";
int THREADS;
vector<fastaSequence*> SORTED_SEQUENCES;
map<int,int> SEARCH_LENGTH_INDEX;

vector<fastaSequence> ReadSequencesFromFastaFile(string path);
void WriteToFastaFile(string path, vector<fastaSequence*> &sequences);
vector<fastaSequence*> filterSequences(vector<fastaSequence> &unsortedSequences, vector<fastaSequence*> &sortedSequences);
vector<vector<fastaSequence*>> SplitVectorIntoChunks(vector<fastaSequence>& sequences, int numberOfChunks = THREADS);
map<int, int>  CreateSequenceLengthSearchIndex(vector<fastaSequence*> sortedRecords);
int GetSearchStartingIndex(const fastaSequence *thisSequence);
int CheckArgumentCount(int argc);
vector<fastaSequence*> GetChunk(vector<fastaSequence> &sequences, int startIndex, long count);
vector<fastaSequence*> recombineChunks(const vector<vector<fastaSequence*>> &chunks);
static bool DoesLongerSequenceExist(const fastaSequence *comparativeSequence, const vector<fastaSequence*>& preSortedSequences);

void DereplicateSequences(vector<fastaSequence*> &thisSequence, const map<int,int> &searchLengthIndex);

bool sortBySequenceLength (const fastaSequence *i, const fastaSequence *j) {
    return (i->sequenceLength < j->sequenceLength);
}

map<int, int> CreateSequenceLengthSearchIndex(vector<fastaSequence*> sortedRecords){
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
    auto initialSequences = ReadSequencesFromFastaFile(inputPath);
    cout << "done" << endl;

    auto initialSequenceCount = initialSequences.size();
    auto startPos = initialSequences.begin();
    SORTED_SEQUENCES = GetChunk(initialSequences, 0, (long) initialSequenceCount);

    cout << "Sorting sequences by length for faster searching..." << endl;
    sort(SORTED_SEQUENCES.begin(), SORTED_SEQUENCES.end(), sortBySequenceLength);
    cout << "done" << endl;

    cout << "Creating sequence length index..." << endl;
    SEARCH_LENGTH_INDEX = CreateSequenceLengthSearchIndex(SORTED_SEQUENCES);
    cout << "done" << endl;

    cout << "Dereplicating sequences..." << endl;
    DereplicateSequences(SORTED_SEQUENCES, SEARCH_LENGTH_INDEX);
    cout << "done" << endl;

    cout << "Original sequence count: " << initialSequenceCount << endl;
    auto filtered = filterSequences(initialSequences, SORTED_SEQUENCES);

    cout << endl << "Final sequence count: "<< filtered.size() << endl;

    cout << "Writing output file" << endl;
    WriteToFastaFile(outputPath, filtered);
    cout << "done" << endl;

    auto stop = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::seconds>(stop - start);
    cout << "Process took: " << duration.count() << " seconds to complete" <<endl;

    return 0;
}

//TODO multithreading required?
void DereplicateSequences(vector<fastaSequence*> &preSortedSequences, const map<int,int> &searchLengthIndex) {
    for(auto &seqIndexItem: searchLengthIndex) {
        set<string> foundSequences = {};
        auto sizeRange = find_if(preSortedSequences.begin()+seqIndexItem.second, preSortedSequences.end(),[&seqIndexItem](fastaSequence* &thisSequence) {
            return thisSequence->sequenceLength > seqIndexItem.first;
        });

        for(auto itr = preSortedSequences.begin()+seqIndexItem.second; itr != sizeRange && itr != preSortedSequences.end(); itr++){
            auto currentSequence = **&itr;
            if(foundSequences.contains(currentSequence->substitutedSequence)){
                currentSequence->eliminated = true;
            }else{
                foundSequences.insert(currentSequence->substitutedSequence);
            }
        }
    }
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
        std::cerr << "usage: "<< PROGRAM_NAME << " <input_file> <output_file> <threads>";
    }
    return result;
}

int GetSearchStartingIndex(const fastaSequence *thisSequence){
    return SEARCH_LENGTH_INDEX[thisSequence->sequenceLength];
}

static bool DoesLongerSequenceExist(const fastaSequence *thisSequence, const vector<fastaSequence*>& preSortedSequences){
    auto startIndex = GetSearchStartingIndex(thisSequence);

    return any_of((preSortedSequences.begin() + startIndex), preSortedSequences.end(), [&thisSequence](const fastaSequence *comparativeSequence){
        if(!comparativeSequence->eliminated){
            return thisSequence->IsShorterVersionOf(comparativeSequence);
        }
        return false;
    });
}

vector<fastaSequence*> GetChunk(vector<fastaSequence> &sequences, int startIndex, long count){
    auto endIndex =  startIndex + count;
    vector<fastaSequence*> result;
    for (int i = startIndex; i < endIndex ;i++){
        auto itemRef = &sequences.at(i);
        result.push_back(itemRef);
    }
    return result;
}

vector<vector<fastaSequence*>> SplitVectorIntoChunks(vector<fastaSequence>& sequences, int numberOfChunks) {
    vector<vector<fastaSequence*>> chunks{};
    auto remainingItemsToChunk = sequences.size();
    int chunkStartPosition = 0;
    for (int chunkIndex = 0; chunkIndex < numberOfChunks; ++chunkIndex) {
        long chunkSize = remainingItemsToChunk / (numberOfChunks - chunkIndex);
        remainingItemsToChunk -= chunkSize;
        chunks.emplace_back(GetChunk(sequences, chunkStartPosition, chunkSize));
        chunkStartPosition += chunkSize;
    }
    return chunks;
}

vector<fastaSequence*> filterSequences(vector<fastaSequence> &unsortedSequences, vector<fastaSequence*> &sortedSequences){
    auto chunkedInput = SplitVectorIntoChunks(unsortedSequences);

    //Make progress bar
    progressbar progressBar(unsortedSequences.size());
    progressBar.set_todo_char(" ");
    progressBar.set_done_char("█");
    progressBar.set_opening_bracket_char("{");
    progressBar.set_closing_bracket_char("}");

    #pragma omp parallel for num_threads(THREADS)
    for(auto &chunk : chunkedInput) {
        for (auto &sequence: chunk) {
            #pragma omp critical
            progressBar.update();
            if (!sequence->eliminated) {
                if (DoesLongerSequenceExist(sequence, sortedSequences)) {
                    sequence->eliminated = true;
                }
            }
        }
    }
    return recombineChunks(chunkedInput);
}

vector<fastaSequence*> recombineChunks(const vector<vector<fastaSequence*>> &chunks){
    vector<fastaSequence*> result;
    for(auto &chunk : chunks) {
        for(auto const &sequence: chunk){
            if(!sequence->eliminated) {
                result.push_back(sequence);
            }
        }
    }
    return result;
}

vector<fastaSequence> ReadSequencesFromFastaFile(string path){
    vector<fastaSequence> fileContent;
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
                fastaSequence newSequenceRecord = {
                        tempLine,
                        tempBuffer
                };
                fileContent.push_back(newSequenceRecord);
                if(!inputFile.eof()){
                    inputFile.putback('>');
                }
            }
        }
        inputFile.close();
    }
    return fileContent;
}

void WriteToFastaFile(string path, vector<fastaSequence*>& sequences){
    ofstream outputFile;
    outputFile.open(path, ios_base::out | ios_base::trunc);
    if(outputFile.is_open()){
        for(auto &sequence : sequences) {
            outputFile << sequence->header << endl;
            outputFile << sequence->originalSequence << endl;
        }
    }
}