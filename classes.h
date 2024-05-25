#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <cmath>

using namespace std;

class Record {
public:
    int id, manager_id; // Employee ID and their manager's ID
    string bio, name; // Fixed length string to store employee name and biography

    Record(int id, int man_id, string name, string bio)
    : id(id), manager_id(man_id), name(name), bio(bio){}

    Record(vector<string> &fields) {
        id = stoi(fields[0]);
        name = fields[1];
        bio = fields[2];
        manager_id = stoi(fields[3]);
    }

    // Function to get the size of the record
    int get_size() {
        return int(sizeof(int) * 4 + bio.length() + name.length());
    }

    // Function to serialize the record for writing to file
    string serialize() const {
        ostringstream oss;
        oss.write(reinterpret_cast<const char *>(&id), sizeof(id));
        oss.write(reinterpret_cast<const char *>(&manager_id), sizeof(manager_id));
        int name_len = name.size();
        int bio_len = bio.size();
        oss.write(reinterpret_cast<const char *>(&name_len), sizeof(name_len));
        oss.write(name.c_str(), name.size());
        oss.write(reinterpret_cast<const char *>(&bio_len), sizeof(bio_len));
        oss.write(bio.c_str(), bio.size());
        return oss.str();
    }

    void print() {
        cout << "\tID: " << id << "\n";
        cout << "\tNAME: " << name << "\n";
        cout << "\tBIO: " << bio << "\n";
        cout << "\tMANAGER_ID: " << manager_id << "\n";
    }
};

class Page {
public:
    vector<Record> records; // Data_Area containing the records
    vector<pair<int, int>> slot_directory; // Slot directory containing offset and size of each record
    int cur_size = 0; // Current size of the page
    int records_size = 0;
    int overflowPointerIndex; // Offset of overflow page, set to -1 by default

    // Constructor
    Page() : overflowPointerIndex(-1) {}

    void print(){
        cout << "# Records: " << records.size() << endl;
        for (auto& r: records) {
            r.print();
        }
    }

    // Function to insert a record into the page
    bool insert_record_into_page(Record r) {
        // cout << "......................................................." << endl;
        // cout << "Inserting record into page" << endl;
        // cout << "Current size: " << cur_size << endl;
        // r.print();
        if (cur_size + r.get_size() + (sizeof(int) * 2) >= 4096) { // Check if page size limit exceeded
            // cout << "Cannot insert, out of space" << endl;
            return false; // Cannot insert the record into this page
        } else {
            records.push_back(r);
            slot_directory.push_back(make_pair(records_size, r.get_size()));

            records_size += r.get_size();
            cur_size += r.get_size() + (sizeof(int) * 2);

            // cout << "New size: " << cur_size << endl;
            // cout << "Records size: " << records_size << endl;
            // cout << "......................................................." << endl;
            return true;
        }
    }

    // Function to write the page to a binary output stream. You may use
    void write_into_data_file(ostream &out) const {
        // Page structure: [ Records | Free space | Slot directory | Overflow index | Num Records ]
        char page_data[4096] = {0}; // Buffer to hold page data
        int offset = 0;
        // cout << "......................................................." << endl;
        // cout << "Writing page data to file" << endl;
        // cout << "Current size: " << cur_size << " Records size: " << records_size << " Slot directory: " << slot_directory.size() << " Records: " << records.size() << endl;

        // Write records into page_data buffer
        for (const auto &record: records) {
            string serialized = record.serialize();
            memcpy(page_data + offset, serialized.c_str(), serialized.size());
            offset += serialized.size();
        }

        // Write the number of records contained in the page
        offset = 0;
        int num_records = slot_directory.size();
        memcpy(page_data + 4096 - sizeof(num_records), &num_records, sizeof(num_records));
        offset += sizeof(num_records);
        // Write overflowPointerIndex into page_data buffer.
        memcpy(page_data + 4096 - sizeof(overflowPointerIndex) - offset, &overflowPointerIndex, sizeof(overflowPointerIndex));
        offset += sizeof(overflowPointerIndex);

        //  You should write the first entry of the slot_directory, which have the info about the first record at the bottom of the page, before overflowPointerIndex.
        for (const auto& slot : slot_directory) {
            // cout << "Recording pair " << slot.first << ", " << slot.second << endl; 
            memcpy(page_data + 4096 - offset - sizeof(slot.first), &slot.first, sizeof(slot.first));
            offset += sizeof(slot.first);
            memcpy(page_data + 4096 - offset - sizeof(slot.second), &slot.second, sizeof(slot.second));
            offset += sizeof(slot.second);
        }
        // cout << "......................................................." << endl;

        // Write the page_data buffer to the output stream
        out.write(page_data, sizeof(page_data));
    }

    // Function to read a page from a binary input stream
    bool read_from_data_file(istream &in) {
        char page_data[4096] = {0}; // Buffer to hold page data
        in.read(page_data, 4096); // Read data from input stream

        // cout << "-------------------------------------------------------" << endl;
        // cout << "Reconstructing page from file" << endl;
        // cout << "Current size: " << cur_size << " Records size: " << records_size << " Slot directory: " << slot_directory.size() << " Records: " << records.size() << endl;
        // cout << "-------------------------------------------------------" << endl;

        streamsize bytes_read = in.gcount();
        if (bytes_read == 4096) {
            int num, offset = 0;
            // Number of records to process
            memcpy(reinterpret_cast<char*>(&num), page_data + 4096 - sizeof(num) - offset, sizeof(num));
            offset += sizeof(num);
            // cout << "Number of records: " << num << " Overflow index: " << overflowPointerIndex << endl;
            // Overflow pointer
            memcpy(reinterpret_cast<char*>(&overflowPointerIndex), page_data + 4096 - sizeof(overflowPointerIndex) - offset, sizeof(overflowPointerIndex));
            offset += sizeof(overflowPointerIndex);

            // Slot directory
            for (int i = 0; i < num; i++) {
                int first, second;
                memcpy(reinterpret_cast<char*>(&first), page_data + 4096 - offset - sizeof(first), sizeof(first));
                offset += sizeof(first);
                // cout << "offset " << offset << " first: " << first << endl;
                memcpy(reinterpret_cast<char*>(&second), page_data + 4096 - offset - sizeof(second), sizeof(second));
                offset += sizeof(second);
                // cout << "offset " << offset << " second: " << second << endl;
                // cout << "Ingested pair " << first << ", " << second << endl;
                cur_size =+ sizeof(int) * 2;
                slot_directory.push_back(make_pair(first, second));
            }

            // Process records
            for (const auto& slot: slot_directory){
                offset = slot.first;
                // int - id
                int id, man_id;
                memcpy(reinterpret_cast<char*>(&id), page_data + offset, sizeof(id));
                offset += sizeof(id);
                // cout << "\t\tid: " << id << endl;
                // int - manager id
                memcpy(reinterpret_cast<char*>(&man_id), page_data + offset, sizeof(man_id));
                offset += sizeof(man_id);
                // cout << "\t\tman id: " << man_id << endl;
                // int - size of name
                int name_size;
                memcpy(reinterpret_cast<char*>(&name_size), page_data + offset, sizeof(name_size));
                offset += sizeof(name_size);
                // cout << "\t\tname size: " << name_size << endl;
                // str - name
                char name[name_size + 1] = {0};
                memcpy(reinterpret_cast<char*>(&name), page_data + offset, name_size);
                offset += name_size;
                // cout << "\t\tname : " << name << endl;
                // int - size of bio
                int bio_size;
                memcpy(reinterpret_cast<char*>(&bio_size), page_data + offset, sizeof(bio_size));
                offset += sizeof(bio_size);
                // cout << "\t\tbio size: " << bio_size << endl;
                // str - bio
                char bio[bio_size + 1] = {0};
                memcpy(reinterpret_cast<char*>(&bio), page_data + offset, bio_size);
                offset += bio_size;
                // cout << "\t\tbio : " << bio << endl;

                Record r(id, man_id, name, bio);
                // cout << "Ingested record: " << endl;
                // r.print();
                // cout << "Adding size " << r.get_size() << endl;
                records.push_back(r);
                cur_size += r.get_size();
                records_size += r.get_size();
                // cout << "Current size: " << cur_size << " Records size: " << records_size << " Slot directory: " << slot_directory.size() << " Records: " << records.size() << endl;
            }

            // cout << "-------------------------------------------------------" << endl;
            // cout << "Finished reconstructing page from file" << endl;
            // cout << "Current size: " << cur_size << " Records size: " << records_size << " Slot directory: " << slot_directory.size() << " Records: " << records.size() << endl;
            // cout << "-------------------------------------------------------" << endl;
            return true;
        }

        if (bytes_read > 0) {
            cerr << "Incomplete read: Expected 4096 bytes, but only read " << bytes_read << " bytes." << endl;
        }

        return false;
    }
};

class HashIndex {
private:
    const size_t maxCacheSize = 1; // Maximum number of pages in the buffer
    const int Page_SIZE = 4096; // Size of each page in bytes
    vector<int> PageDirectory{vector<int>(256,-1)}; // Map h(id) to a bucket location in EmployeeIndex(e.g., the jth bucket)
    // EmployeeIndex is the name of the file we're writing out to
    // can scan to correct bucket using j*Page_SIZE as offset (using seek function)
    // can initialize to a size of 256 (assume that we will never have more than 256 regular (i.e., non-overflow) buckets)
    int nextFreePage; // Next place to write a bucket
    string fileName;

    // Function to compute hash value for a given ID
    int compute_hash_value(int id) {
        return id % (int)pow(2, 8);
    }

    // Function to add a new record to an existing page in the index file
    void addRecordToIndex(int pageIndex,/* Page &page,*/ Record &record) {
        // Open index file in binary mode for updating
        fstream indexFile;
        indexFile.open(fileName, ios::binary | ios::out | ios::in);// | ios::trunc);

        if (!indexFile) {
            cerr << "Error: Unable to open index file for adding record." << endl;
            return;
        }

        // Seek to the appropriate position in the index file
        Page page;
        indexFile.seekp(pageIndex * Page_SIZE, ios::beg);
        // Load in that page and attempt to insert
        page.read_from_data_file(indexFile);

        // Check if the page has overflow
        while (page.overflowPointerIndex != -1) {
            // cout << "\tPage " << pageIndex << " has overflow, moving to " << page.overflowPointerIndex << endl;
            pageIndex = page.overflowPointerIndex;
            indexFile.seekp(pageIndex * Page_SIZE, ios::beg);
            // page.read_from_data_file(indexFile);
            Page t;
            t.read_from_data_file(indexFile);
            page = t;
        }

        if( page.insert_record_into_page(record)){
            // cout << "Successfully inserted record, writing to file" << endl;
            indexFile.seekp(pageIndex * Page_SIZE, ios::beg);
            page.write_into_data_file(indexFile);
        } else {
            // If we failed to insert it, we need to create an overflow page
            // cout << "Failed to insert record, creating overflow page at index " << nextFreePage << endl;
            page.overflowPointerIndex = nextFreePage;
            nextFreePage++;

            Page p;
            if(! p.insert_record_into_page(record)){
                cout << "ERROR, couldn't insert into overflow page" << endl;
                return;
            }
            // Write our original page with its new overflow index
            indexFile.seekp(pageIndex * Page_SIZE, ios::beg);
            page.write_into_data_file(indexFile);

            // Write the new overflow page
            indexFile.seekp(page.overflowPointerIndex * Page_SIZE, ios::beg);
            p.write_into_data_file(indexFile);
        }

        // Close the index file
        indexFile.close();

    }

    // Function to search for a record by ID in a given page of the index file
    void searchRecordByIdInPage(int pageIndex, int id) {
        // Open index file in binary mode for reading
        fstream indexFile(fileName, ios::binary | ios::out | ios::in);

        // Seek to the appropriate position in the index file
        indexFile.seekp(pageIndex * Page_SIZE, ios::beg);

        // Read the page from the index file
        Page page;
        page.read_from_data_file(indexFile);
        // page.print();

        // TODO:
        //  - Search for the record by ID in the page
        for (auto& r: page.records) {
            // cout << "\tSearching ID " << r.id << endl;
            if(r.id == id){
                cout << "Found match: " << endl;
                r.print();
                return;
            }
        }
        //  - Check for overflow pages and report if record with given ID is not found
        if(page.overflowPointerIndex != -1){
            // Search the overflow page
            // cout << "Searching overflow page with index " << page.overflowPointerIndex << endl;
            searchRecordByIdInPage(page.overflowPointerIndex, id);
            return;
        }
        cout << "ID " << id << " not found. " << endl;
    }

public:
    HashIndex(string indexFileName) : nextFreePage(0), fileName(indexFileName) { }

    // Function to create hash index from Employee CSV file
    void createFromFile(string csvFileName) {
        // Read CSV file and add records to index
        // Open the CSV file for reading
        ifstream csvFile(csvFileName);

        string line;
        // cout << "======================================================================" << endl;
        // cout << "Creating data file" << endl;
        // cout << "======================================================================" << endl;

        // Start by writing out one blank page so we aren't working with an empty file        
        Page p;
        fstream indexFile;
        indexFile.open(fileName, ios::binary | ios::out | ios::in | ios::trunc);
        indexFile.seekp(0, ios::beg);
        p.write_into_data_file(indexFile);
        indexFile.close();

        while (getline(csvFile, line)) {
            stringstream ss(line);
            string item;
            vector<string> fields;
            while (getline(ss, item, ',')) {
                fields.push_back(item);
            }
            Record record(fields);

            int hash_value = compute_hash_value(record.id);
            // Get the page index from PageDirectory. If it's not in PageDirectory, define a new page using nextFreePage.
            int page_index = PageDirectory.at(hash_value);
            // cout << "Hash value is " << hash_value << " with page index "<< page_index;
            if(page_index == -1){
                // If it doesn't exist, create it
                Page p;
                page_index = nextFreePage;
                PageDirectory.at(hash_value) = page_index;
                nextFreePage++;
                int loc = page_index * Page_SIZE;
                // cout << ". Creating new page with index " << page_index << " at position " << loc << endl;
                p.insert_record_into_page(record);
                // write this to the proper spot in the file
                fstream indexFile;
                indexFile.open(fileName, ios::binary | ios::out | ios::in);//  | ios::trunc);
                indexFile.seekp(page_index * Page_SIZE, ios::beg);
                p.write_into_data_file(indexFile);
                indexFile.close();
            } else {
                // If it does exist, get it
                // cout << ". Fetching existing page" << endl;
                //   - Insert the record into the appropriate page in the index file using addRecordToIndex() function.
                addRecordToIndex(page_index, record);
            }
        }
        // Close the CSV file
        csvFile.close();
        // cout << "======================================================================" << endl;
        // cout << "Finished creating data file" << endl;
        // cout << "======================================================================" << endl;
    }

    // Function to search for a record by ID in the hash index
    void findAndPrintEmployee(int id) {
        // Open index file in binary mode for reading
        // ifstream indexFile(fileName, ios::binary | ios::in);

        //  - Compute hash value for the given ID using compute_hash_value() function
        int hash_value = compute_hash_value(id);
        // Using the hash value, get the right page
        int pageIndex = PageDirectory.at(hash_value);
        //  - Search for the record in the page corresponding to the hash value using searchRecordByIdInPage() function
        // cout << "Hash for target employee: " << hash_value << " Page index: " << pageIndex << endl;
        searchRecordByIdInPage(pageIndex, id);

        // Close the index file
        // indexFile.close();
    }
};

