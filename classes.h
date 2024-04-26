#include <string>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <bitset>
using namespace std;

class Record {
public:
    int id, manager_id; 
    std::string bio, name; 
    vector<int> offsets; // A vector to store offsets of fields

    // Constructor with parameters for ID, manager ID, name, bio, and offsets
    Record(int id, int manager_id, const string& name, const string& bio, const vector<int>& offsets)
        : id(id), manager_id(manager_id), name(name), bio(bio), offsets(offsets) {}
    
    // Alternative constructor with a different parameter order
    Record(int id, const std::string& name, const std::string& bio, int manager_id)
        : id(id), manager_id(manager_id), name(name), bio(bio) {}
        
    // Constructor that initializes Record from a vector of strings
    Record(vector<std::string> fields) {
        if (fields.size() < 4) {
            throw std::invalid_argument("Not enough fields provided to create a Record."); // Check for sufficient fields
        }
        id = stoi(fields[0]); // Convert the first field to an integer ID
        manager_id = stoi(fields[3]); // Convert the fourth field to an integer manager ID

        // Assume record starts at the first byte after ID.
        // Adjustments may be needed if there is additional metadata, such as record length.
        int currentOffset = sizeof(int) * 2; // ID and manager_id each take 4 bytes

        // Calculate the offset for the name field and add it to the offsets vector
        offsets.push_back(currentOffset);
        name = fields[1]; // Assign the second field to name
        currentOffset += name.length() + 1; // Add the length of name and a null terminator to the current offset

        // Calculate the offset for the bio field and add it to the offsets vector
        offsets.push_back(currentOffset);
        bio = fields[2]; // Assign the third field to bio
    }

    // Method to print record details
    void print() {
        cout << "\tID: " << id << "\n"; 
        cout << "\tNAME: " << name << "\n";
        cout << "\tBIO: " << bio << "\n";
        cout << "\tMANAGER_ID: " << manager_id << "\n";
    }
};



class StorageBufferManager {

private:
    static const int PAGE_SIZE = 4096; // Define the size of a page as 4096 bytes
    char pageBuffer[PAGE_SIZE]; // Define a buffer to store a page of data
    fstream dataFile; // File stream for reading from and writing to the data file
    int currentPage; 
    int currentOffset; 
    int writeIntCount = 0; 
    int writeRecordCount = 0; 
    int WriteTotal = 0; 

    // Function to write an integer to the dataFile
    void writeInt(int value) {
        int freeSpacePointer; // Define a pointer to track free space in the page
        // Read the free space pointer from the end of the page
        memcpy(&freeSpacePointer, pageBuffer + PAGE_SIZE - 8, sizeof(int));
        // Write the integer value at the position pointed by the free space pointer
        memcpy(pageBuffer + freeSpacePointer, &value, sizeof(int));
        // Update the free space pointer to reflect the space consumed
        freeSpacePointer += sizeof(int);
        // Write the updated free space pointer back to the end of the page
        memcpy(pageBuffer + PAGE_SIZE - 8, &freeSpacePointer, sizeof(int));
    }

    // Function to read an integer from the dataFile
    int readInt() {
        int value; // Variable to store the read integer
        dataFile.read(reinterpret_cast<char*>(&value), sizeof(int)); // Read an integer from the file
        // cout << "ReadInt" << endl;
        return value; // Return the read integer
    }

    // Function to write a string to the dataFile
    void writeString(const string& str) {
        int freeSpacePointer; // Define a pointer to track free space in the page
        // Read the free space pointer from the end of the page
        memcpy(&freeSpacePointer, pageBuffer + PAGE_SIZE - 8, sizeof(int));
        // Write the string and its null terminator at the position pointed by the free space pointer
        memcpy(pageBuffer + freeSpacePointer, str.c_str(), str.size() + 1);
        freeSpacePointer += str.size() + 1; // Update the free space pointer to include the string and null terminator
        // Write the updated free space pointer back to the end of the page
        memcpy(pageBuffer + PAGE_SIZE - 8, &freeSpacePointer, sizeof(int));
    }

    // Function to read a string from the dataFile
    string readString() {
        std::stringstream strStream; // Use a stringstream to construct the string
        char ch; // Variable to hold each character read
        while (dataFile.get(ch) && ch != '\0') { // Read characters until a null terminator is encountered
            strStream << ch; // Append each character to the stringstream
        }
        return strStream.str(); // Return the constructed string
    }

    // Function to read a record from the dataFile based on offset and length
    Record readRecord(int offset) {
        // Set the file stream's position to the specified offset
        dataFile.seekg(offset);

        // Read id and manager_id
        int id = readInt(); // Read the ID
        int manager_id = readInt(); // Read the manager's ID

        // Read the offsets for the name and bio fields
        vector<int> fieldOffsets;
        fieldOffsets.push_back(dataFile.tellg()); // Current offset for the name field

        // Read the name field
        string name = readString();

        // Record the offset for the bio field
        fieldOffsets.push_back(dataFile.tellg()); // Current offset for the bio field

        // Read the bio field
        string bio = readString();

        // Create and return a Record object with the read fields and their offsets
        return Record(id, manager_id, name, bio, fieldOffsets);
    }


    void writeAndInsertRecord(const Record& record) {
        int freeSpacePointer, slotDirectoryCount;

        // Read the free space pointer and the number of slot directory entries from the end of the page
        memcpy(&freeSpacePointer, pageBuffer + PAGE_SIZE - sizeof(int) * 2, sizeof(int));
        memcpy(&slotDirectoryCount, pageBuffer + PAGE_SIZE - sizeof(int), sizeof(int));

        cout << "Free Space Pointer: " << freeSpacePointer << endl;
        cout << "Number of Slot Directory Entries: " << slotDirectoryCount << endl;

        // Calculate the total size of the record including the size of each field plus null terminators
        int recordSize = sizeof(record.id) +
                        record.name.size() + 1 + record.bio.size() + 1 + 
                        sizeof(record.manager_id); // Include null terminators

        // Ensure there is enough space on the page for the new record and a slot directory entry
        if (freeSpacePointer + recordSize > PAGE_SIZE - sizeof(int) * 2 - (slotDirectoryCount + 1) * sizeof(int) * 2) {
            // If not enough space, flush the current page to the file and initialize a new page
            flushPageToFile();
            initializeNewPage();
            freeSpacePointer = 0; // Reset free space pointer to the beginning
            slotDirectoryCount = 0; // Reset slot directory entry count
            cout << "-------------- FlushPageToFile --------------" << endl;
        }

        // Write the record to the page buffer
        int recordStartOffset = freeSpacePointer; // Starting offset for the record
        memcpy(pageBuffer + freeSpacePointer, &record.id, sizeof(record.id));
        freeSpacePointer += sizeof(record.id);

        memcpy(pageBuffer + freeSpacePointer, record.name.c_str(), record.name.size() + 1); // Include null terminator
        freeSpacePointer += record.name.size() + 1;

        memcpy(pageBuffer + freeSpacePointer, record.bio.c_str(), record.bio.size() + 1); // Include null terminator
        freeSpacePointer += record.bio.size() + 1;

        memcpy(pageBuffer + freeSpacePointer, &record.manager_id, sizeof(record.manager_id));
        freeSpacePointer += sizeof(record.manager_id);

        // Update the slot directory with the record's start offset and size
        int slotDirectoryOffset = PAGE_SIZE - sizeof(int) * 2 - (slotDirectoryCount + 1) * sizeof(int) * 2;
        memcpy(pageBuffer + slotDirectoryOffset, &recordStartOffset, sizeof(int)); // Record start offset
        memcpy(pageBuffer + slotDirectoryOffset + sizeof(int), &recordSize, sizeof(int)); // Record size

        // Update the number of slot directory entries and the free space pointer
        slotDirectoryCount++;
        memcpy(pageBuffer + PAGE_SIZE - sizeof(int), &slotDirectoryCount, sizeof(int));
        memcpy(pageBuffer + PAGE_SIZE - sizeof(int) * 2, &freeSpacePointer, sizeof(int));
        WriteTotal++;
        cout << "Write and Insert:" << WriteTotal << endl;
        cout << "\n" << endl;
    }

    bool pageBufferIsEmpty() {
        // Check if the free space pointer points to the start of the page, indicating an empty page buffer
        int freeSpacePointer;
        memcpy(&freeSpacePointer, pageBuffer + PAGE_SIZE - sizeof(int) * 2, sizeof(int));
        return freeSpacePointer == 0;
    }

    // Ensure the last page record could be flushed
    void finalizeWrite() {
        if (!pageBufferIsEmpty()) {
            flushPageToFile();
        }
    }

    void flushPageToFile() {
        dataFile.seekp(0, ios::end); // Move to the end of the file
        dataFile.write(pageBuffer, PAGE_SIZE); // Write the complete PAGE_SIZE
        dataFile.flush(); // Ensure data is written to disk
        memset(pageBuffer, 0, PAGE_SIZE); // Clear pageBuffer for the next page of data
    }

    void initializeNewPage() {
        memset(pageBuffer, 0, PAGE_SIZE); // Clear the page
        int freeSpacePointer = 0; // Position where data starts
        int slotDirectoryCount = 0; // Initial number of slot directory entries is 0

        // Store the free space pointer and the number of slot directory entries at the end of the page
        memcpy(pageBuffer + PAGE_SIZE - 8, &freeSpacePointer, sizeof(int));
        memcpy(pageBuffer + PAGE_SIZE - 4, &slotDirectoryCount, sizeof(int));
    }

    void updateSlotDirectory(int count, int offset, int length) {
        int slotDirectoryOffset = PAGE_SIZE - 8 - count * sizeof(int) * 2;
        // Assuming the offset is the starting point of the record and length is the size of the record
        memcpy(pageBuffer + slotDirectoryOffset, &offset, sizeof(int));
        memcpy(pageBuffer + slotDirectoryOffset + sizeof(int), &length, sizeof(int));

        // Remember to update the slotDirectoryCount and freeSpacePointer in the pageBuffer if necessary
    }


public:
    // Constructor for StorageBufferManager that opens a binary file for both input and output
    StorageBufferManager(const string& newFileName) {
        // Try to open the file in both read and write mode
        dataFile.open(newFileName, ios::in | ios::out | ios::binary);

        // Check if the file opening failed, indicating the file does not exist
        if (!dataFile.is_open()) {
            // Create a new file for output then close and reopen for both input and output
            dataFile.open(newFileName, ios::out | ios::binary);
            dataFile.close(); // Close the file after creation
            dataFile.open(newFileName, ios::in | ios::out | ios::binary); // Reopen for reading and writing
        }

        currentPage = 0; 
        currentOffset = 0; 
    }

    // Reads records from a CSV file and adds them to the data file
    void createFromFile(string csvFName) {
        ifstream csvFile(csvFName); // Open the CSV file for reading
        string line;

        // Check if the file opening failed
        if (!csvFile.is_open()) {
            cerr << "Failed to open file: " << csvFName << endl; // Print an error message
            return; // Exit the function
        }

        // Read lines from the CSV file until the end
        while (getline(csvFile, line)) {
            stringstream ss(line); // Use stringstream for parsing the line
            string field;
            vector<string> fields; // Store each field in a vector

            // Parse the line into fields separated by commas
            while (getline(ss, field, ',')) {
                fields.push_back(field); // Add each field to the vector
            }

            // Check if the number of fields matches the expected format
            if (fields.size() == 4) {
                Record record(fields); // Create a Record object from the fields

                // Write and insert the record into the data file
                writeAndInsertRecord(record);
            } else {
                cerr << "Invalid record format: " << line << endl; // Print an error message for invalid records
            }
            
        }
        // Ensure the last page's record is flushed to the file
        finalizeWrite();
        csvFile.close(); 
    }

    // Reads a string from the data file at a specified offset
    string readStringAtOffset(fstream& dataFile, int offset) {
        string result; // String to store the result
        char ch; // Character variable for reading one character at a time

        // Set the file position to the specified offset
        dataFile.seekg(offset);

        // Read characters until a null terminator is encountered
        while (dataFile.get(ch) && ch != '\0') {
            result += ch; // Append each character to the result string
        }

        return result; // Return the constructed string
    }

    // Given an ID, find the relevant record and print it
    void findRecordById(int searchId) {
        // Check if the data file is open before proceeding
        if (!dataFile.is_open()) {
            cerr << "Error: File is not open." << endl; // Print an error if the file isn't open
            return; // Exit the function
        }

        // Move to the end of the file to determine its size
        dataFile.seekg(0, ios::end);
        long fileSize = dataFile.tellg(); // Get the file size
        // Calculate the number of pages in the file, rounding up to include partially filled pages
        int pageCount = (fileSize + PAGE_SIZE - 1) / PAGE_SIZE;

        // Iterate through each page in the file
        for (int pageNum = 0; pageNum <= pageCount; pageNum++) {
            int currentPageStart = pageNum * PAGE_SIZE; // Calculate the start of the current page
            dataFile.seekg(currentPageStart, ios::beg); // Move to the start of the current page

            // Read the slot directory count from the end of the page
            int slotDirectoryCount;
            dataFile.seekg(currentPageStart + PAGE_SIZE - 4, ios::beg);
            dataFile.read(reinterpret_cast<char*>(&slotDirectoryCount), sizeof(slotDirectoryCount));

            // Calculate the start of the slot directory based on its count
            int slotDirectoryStart = currentPageStart + PAGE_SIZE - 8 - slotDirectoryCount * sizeof(int) * 2;

            // Iterate through each slot in the directory
            for (int i = 0; i < slotDirectoryCount; i++) {
                int slotEntryOffset = slotDirectoryStart + i * sizeof(int) * 2; // Calculate the offset for the current slot entry
                dataFile.seekg(slotEntryOffset, ios::beg); // Move to the slot entry

                int recordOffset, recordLength;
                dataFile.read(reinterpret_cast<char*>(&recordOffset), sizeof(recordOffset)); // Read the offset of the record
                dataFile.read(reinterpret_cast<char*>(&recordLength), sizeof(recordLength)); // Read the length of the record
                recordOffset += currentPageStart; // Adjust the record offset to account for the current page

                // Seek to the record's offset to begin reading its contents
                dataFile.seekg(recordOffset, ios::beg);
                int id;
                dataFile.read(reinterpret_cast<char*>(&id), sizeof(id)); // Read the record's ID
                if (id == searchId) { // Check if the current record's ID matches the search ID
                    // Calculate the offsets for the name and bio fields
                    int nameOffset = recordOffset + sizeof(int); // Skip past the ID
                    int bioOffset = nameOffset + readStringAtOffset(dataFile, nameOffset).length() + 1; // Skip past the name and its null terminator

                    // Read the name and bio strings
                    string name = readStringAtOffset(dataFile, nameOffset); // Use the readStringAtOffset function
                    string bio = readStringAtOffset(dataFile, bioOffset); // Use the readStringAtOffset function

                    // Calculate the offset for the manager_id and read it
                    int manager_idOffset = bioOffset + bio.length() + 1; // Skip past the bio and its null terminator
                    dataFile.seekg(manager_idOffset);
                    int manager_id;
                    dataFile.read(reinterpret_cast<char*>(&manager_id), sizeof(manager_id));

                    // Create a Record instance with the found data and print it
                    Record foundRecord(id, name, bio, manager_id);
                    foundRecord.print();
                    return; // Exit the function after finding and printing the record
                }
            }
        }

        // Print a message if the record with the given ID was not found
        cout << "Record with ID " << searchId << " not found." << endl;
    }

};