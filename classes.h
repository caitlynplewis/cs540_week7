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
    int overflowPointerIndex; // Offset of overflow page, set to -1 by default

    // Constructor
    Page() : overflowPointerIndex(-1) {}

    // Function to insert a record into the page
    bool insert_record_into_page(Record r) {
        if (cur_size + r.get_size() >= 4096) { // Check if page size limit exceeded
            return false; // Cannot insert the record into this page
        } else {
            records.push_back(r);
            slot_directory.push_back(make_pair(cur_size, r.get_size()));
            cur_size += r.get_size();
            return true;
        }
    }

    // Function to write the page to a binary output stream. You may use
    void write_into_data_file(ostream &out) const {
        // Page structure: [ Records | Free space | Slot directory | Overflow index ]
        char page_data[4096] = {0}; // Buffer to hold page data
        int offset = 0;

        // Write records into page_data buffer
        for (const auto &record: records) {
            string serialized = record.serialize();
            memcpy(page_data + offset, serialized.c_str(), serialized.size());
            offset += serialized.size();
        }

        // Write the number of records contained in the page
        int num_records = slot_directory.size();
        memcpy(page_data + 4096 - sizeof(num_records), &num_records, sizeof(num_records));
        // Write overflowPointerIndex into page_data buffer.
        memcpy(page_data + 4096 - sizeof(overflowPointerIndex), &overflowPointerIndex, sizeof(overflowPointerIndex));

        //  You should write the first entry of the slot_directory, which have the info about the first record at the bottom of the page, before overflowPointerIndex.
        offset = sizeof(overflowPointerIndex) + sizeof(num_records);
        for (const auto& slot : slot_directory) {
            // cout << "Recording pair " << slot.first << ", " << slot.second << endl; 
            memcpy(page_data + 4096 - offset - sizeof(slot.first), &slot.first, sizeof(slot.first));
            offset += sizeof(slot.first);
            memcpy(page_data + 4096 - offset - sizeof(slot.second), &slot.second, sizeof(slot.second));
            offset += sizeof(slot.second);
        }

        // Write the page_data buffer to the output stream
        out.write(page_data, sizeof(page_data));
    }

    // Function to read a page from a binary input stream
    bool read_from_data_file(istream &in) {
        char page_data[4096] = {0}; // Buffer to hold page data
        in.read(page_data, 4096); // Read data from input stream

        streamsize bytes_read = in.gcount();
        if (bytes_read == 4096) {
            int num, offset = 0;
            // Number of records to process
            memcpy(reinterpret_cast<char*>(&num), page_data + 4096 - sizeof(num), sizeof(num));
            offset += sizeof(num);
            // Overflow pointer
            memcpy(reinterpret_cast<char*>(&overflowPointerIndex), page_data + 4096 - sizeof(overflowPointerIndex) - offset, sizeof(overflowPointerIndex));
            // Slot directory
            for (int i = 0; i < num; i++) {
                int first, second;
                memcpy(reinterpret_cast<char*>(&first), page_data + 4096 - offset, sizeof(first));
                offset += sizeof(first);
                memcpy(reinterpret_cast<char*>(&second), page_data + 4096 - offset, sizeof(second));
                offset += sizeof(second);
                // cout << "Made pair " << first << ", " << second << endl;
                slot_directory.push_back(make_pair(first, second));
            }

            // Process records
            for (const auto& slot: slot_directory){
                offset = slot.first;
                // int - id
                int id, man_id;
                memcpy(reinterpret_cast<char*>(&id), page_data + offset, sizeof(id));
                offset += sizeof(id);
                // int - manager id
                memcpy(reinterpret_cast<char*>(&man_id), page_data + offset, sizeof(man_id));
                offset += sizeof(man_id);
                // int - size of name
                int name_size;
                memcpy(reinterpret_cast<char*>(&name_size), page_data + offset, sizeof(name_size));
                offset += sizeof(name_size);
                // str - name
                char name[name_size + 1] = {0};
                memcpy(reinterpret_cast<char*>(&name), page_data + offset, name_size);
                offset += name_size;
                // int - size of bio
                int bio_size;
                memcpy(reinterpret_cast<char*>(&bio_size), page_data + offset, sizeof(bio_size));
                offset += sizeof(bio_size);
                // str - bio
                char bio[bio_size + 1] = {0};
                memcpy(reinterpret_cast<char*>(&bio), page_data + offset, bio_size);
                offset += bio_size;

                Record r(id, man_id, name, bio);
                // r.print();
                records.push_back(r);
            }

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
    void addRecordToIndex(int pageIndex, Page &page, Record &record) {
        // Open index file in binary mode for updating
        fstream indexFile(fileName, ios::binary | ios::in | ios::out);

        if (!indexFile) {
            cerr << "Error: Unable to open index file for adding record." << endl;
            return;
        }
        // Check if the page has overflow
        while (page.overflowPointerIndex != -1) {
            pageIndex = page.overflowPointerIndex;
            indexFile.seekp(pageIndex * Page_SIZE, ios::beg);
            page.read_from_data_file(indexFile);
        }

        // Now that we've followed all the overflow, insert it here
        // Seek to the appropriate position in the index file
        indexFile.seekp(pageIndex * Page_SIZE, ios::beg);
        // Load in that page and attempt to insert
        page.read_from_data_file(indexFile);
        if( page.insert_record_into_page(record)){
            indexFile.seekp(pageIndex * Page_SIZE, ios::beg);
            page.write_into_data_file(indexFile);
        } else {
            // If we failed to insert it, we need to create an overflow page
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
        ifstream indexFile(fileName, ios::binary | ios::in);

        // Seek to the appropriate position in the index file
        indexFile.seekg(pageIndex * Page_SIZE, ios::beg);

        // Read the page from the index file
        Page page;
        page.read_from_data_file(indexFile);

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
            searchRecordByIdInPage(page.overflowPointerIndex, id);
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
        // Read each line from the CSV file
        while (getline(csvFile, line)) {
            // Parse the line and create a Record object
            stringstream ss(line);
            string item;
            vector<string> fields;
            while (getline(ss, item, ',')) {
                fields.push_back(item);
            }
            Record record(fields);
            // cout << "Created record to insert: " << endl;
            // record.print();

            int hash_value = compute_hash_value(record.id);
            // Get the page index from PageDirectory. If it's not in PageDirectory, define a new page using nextFreePage.
            int page_index = PageDirectory.at(hash_value);
            cout << "Hash value is " << hash_value << " with page index "<< page_index << endl;
            Page p;
            if(page_index == -1){
                // If it doesn't exist, create it
                cout << "\tCreating new page" << endl;
                page_index = nextFreePage;
                PageDirectory.at(hash_value) = page_index;
                nextFreePage++;
            } else {
                // If it does exist, get it
                cout << "\tFetching existing page" << endl;
                fstream indexFile(fileName, ios::binary | ios::in | ios::out);
                p.read_from_data_file(indexFile);
                indexFile.close();
            }

            //   - Insert the record into the appropriate page in the index file using addRecordToIndex() function.
            addRecordToIndex(page_index, p, record);
        }

        // Close the CSV file
        csvFile.close();
    }

    // Function to search for a record by ID in the hash index
    void findAndPrintEmployee(int id) {
        // Open index file in binary mode for reading
        ifstream indexFile(fileName, ios::binary | ios::in);

        //  - Compute hash value for the given ID using compute_hash_value() function
        int hash_value = compute_hash_value(id);
        // Using the hash value, get the right page
        int pageIndex = PageDirectory.at(hash_value);
        //  - Search for the record in the page corresponding to the hash value using searchRecordByIdInPage() function
        searchRecordByIdInPage(pageIndex, id);

        // Close the index file
        indexFile.close();
    }
};

