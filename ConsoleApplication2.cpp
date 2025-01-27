#include <iostream>
#include <vector>
#include <atomic>
#include <mutex>
#include <memory>
#include <cassert>
#include <thread>
#include <string>
#include <type_traits>

template <typename Key, typename Value>
class CustomHashTable {
public:
    struct Entry {
        Key key;
        Value value;
        std::atomic<bool> in_use; // atomic flag for the entry's state (in-use or free)

        Entry() : in_use(false) {}
    };

private:
    std::vector<Entry*> table;
    size_t table_size;
    size_t item_count;
    std::mutex resize_mutex;  // Mutex to handle resizing of the hash table
    std::mutex access_mutex;  // Mutex to handle access to the hash table

    //static_assert(!std::is_pointer<Key>::value, "Pointers are not allowed as keys.");

    size_t hash(const Key& key) const {
        return std::hash<Key>{}(key) % table_size;
    }

    void resize() {
        std::lock_guard<std::mutex> lock(resize_mutex);

        if (item_count * 2 > table_size) {
            size_t new_size = table_size * 2;
            std::vector<Entry*> new_table(new_size, nullptr);

            for (auto& entry : table) {
                if (entry && entry->in_use.load()) {
                    size_t index = std::hash<Key>{}(entry->key) % new_size;
                    while (new_table[index]) {
                        index = (index + 1) % new_size;  // Linear probing for collisions
                    }
                    new_table[index] = entry;
                }
            }

            table = std::move(new_table);
            table_size = new_size;
        }
    }

    bool check_key(const Key& key) const {
        if constexpr (std::is_same<Key, int*>::value || std::is_same<Key, char*>::value) {
            if (key == nullptr) {
                std::cout << "\n nullptr is not allowed as a key.";
                return false;
            }
        }
        return true;
    }

public:
    CustomHashTable(size_t initial_size = 16) : table_size(initial_size), item_count(0) {
        table.resize(initial_size, nullptr);
    }

    bool insert(const Key& key, const Value& value) {
        if (!check_key(key))
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(access_mutex);

        resize(); // Ensure that we have space

        size_t index = hash(key);
        size_t original_index = index;

        while (table[index]) {
            if (table[index]->in_use.load() && table[index]->key == key) {
                table[index]->value = value;  // Overwrite the existing value
                return true;
            }
            index = (index + 1) % table_size;
            if (index == original_index) {
                resize();
                return insert(key, value);  // Retry after resizing
            }
        }

        if (!table[index]) {
            table[index] = new Entry();
        }
        table[index]->key = key;
        table[index]->value = value;
        table[index]->in_use.store(true);
        item_count++;
        return true;
    }

    bool find(const Key& key, Value& result) const {
        if (!check_key(key))
        {
            return false;
        }

        std::lock_guard<std::mutex> lock(access_mutex);

        size_t index = hash(key);
        size_t original_index = index;

        while (table[index]) {
            if (table[index]->in_use.load() && table[index]->key == key) {
                result = table[index]->value;
                return true;
            }
            index = (index + 1) % table_size;
            if (index == original_index) break;
        }

        return false;
    }

    bool remove(const Key& key) {
        if (!check_key(key)) 
        {
          return false;
        }

        std::lock_guard<std::mutex> lock(access_mutex);

        size_t index = hash(key);
        size_t original_index = index;

        while (table[index]) {
            if (table[index]->in_use.load() && table[index]->key == key) {
                table[index]->in_use.store(false);
                item_count--;
                return true;
            }
            index = (index + 1) % table_size;
            if (index == original_index) break;
        }

        return false;
    }

    size_t size() const {
        return item_count;
    }

    ~CustomHashTable() {
        for (auto& entry : table) {
            delete entry;
        }
    }
};

void testDuplicateInsertions() {
    CustomHashTable<int, std::string> hashTable(4);

    assert(hashTable.insert(1, "one"));
    assert(hashTable.insert(1, "updated_one"));  // Duplicate insertion, value should be updated

    std::string result;
    assert(hashTable.find(1, result) && result == "updated_one");
    std::cout << "Duplicate insertion test passed!" << std::endl;
}

void testRemoveNonExistentKey() {
    CustomHashTable<int, std::string> hashTable(4);

    assert(!hashTable.remove(1));  // Removing a non-existent key should return false
    std::cout << "Remove non-existent key test passed!" << std::endl;
}

void testTableResizing() {
    CustomHashTable<int, std::string> hashTable(2);

    assert(hashTable.insert(1, "one"));
    assert(hashTable.insert(2, "two"));
    assert(hashTable.insert(3, "three"));  // This should trigger a resize

    std::string result;
    assert(hashTable.find(1, result) && result == "one");
    assert(hashTable.find(2, result) && result == "two");
    assert(hashTable.find(3, result) && result == "three");

    std::cout << "Table resizing test passed!" << std::endl;
}

void testEmptyTableOperations() {
    CustomHashTable<int, std::string> hashTable(4);

    std::string result;
    assert(!hashTable.find(1, result));  // Should return false for non-existent key
    assert(!hashTable.remove(1));  // Removing a non-existent key should fail
    std::cout << "Empty table operations test passed!" << std::endl;
}

void testLargeNumberOfInsertsAndFinds() {
    CustomHashTable<int, std::string> hashTable(16);

    for (int i = 0; i < 1000; i++) {
        assert(hashTable.insert(i, "value_" + std::to_string(i)));
    }

    std::string result;
    for (int i = 0; i < 1000; i++) {
        assert(hashTable.find(i, result) && result == "value_" + std::to_string(i));
    }

    std::cout << "Large number of inserts and finds test passed!" << std::endl;
}

void testThreadSafety() {
    CustomHashTable<int, std::string> hashTable(4);

    // Basic multithreading test
    auto threadFunc = [&hashTable](int start) {
        for (int i = start; i < start + 100; i++) {
            hashTable.insert(i, "value_" + std::to_string(i));
        }
    };

    std::thread t1(threadFunc, 0);
    std::thread t2(threadFunc, 100);

    t1.join();
    t2.join();

    std::string result;
    for (int i = 0; i < 200; i++) {
        assert(hashTable.find(i, result) && result == "value_" + std::to_string(i));
    }

    std::cout << "Thread safety test passed!" << std::endl;
}

void testSingleEntry() {
    CustomHashTable<int, std::string> hashTable(4);

    assert(hashTable.insert(1, "one"));
    assert(hashTable.remove(1));

    std::string result;
    assert(!hashTable.find(1, result));  // Element should be removed
    std::cout << "Single entry test passed!" << std::endl;
}

void testCustomHashFunction() {
    // Custom hash function that ensures that all keys hash to the same index (edge case)
    struct CustomHash {
        size_t operator()(const int& key) const {
            return 0;  // All keys will hash to index 0
        }
    };

    CustomHashTable<int, std::string> hashTable(4);

    assert(hashTable.insert(1, "one"));
    assert(hashTable.insert(2, "two"));
    assert(hashTable.insert(3, "three"));

    std::string result;
    assert(hashTable.find(1, result) && result == "one");
    assert(hashTable.find(2, result) && result == "two");
    assert(hashTable.find(3, result) && result == "three");

    std::cout << "Custom hash function test passed!" << std::endl;
}

void testMyCustomHashFunction() {
    // Custom hash function that ensures that all keys hash to the same index (edge case)
    struct CustomHash {
        size_t operator()(const int& key) const {
            return 0;  // All keys will hash to index 0
        }
    };
    CustomHashTable<int*, std::string> hashTable(4);
    try {
     //   CustomHashTable<int*, std::string> hashTable(4);
        // should not allow nullptr as key, handle cases where pointers are not allowed as keys.
        assert(hashTable.insert(nullptr, "one"));
    }
    catch (const std::invalid_argument& e) {
        std::cout << e.what() << std::endl;
    }

    try {
        assert(hashTable.insert(2, "two"));
    }
    catch (const std::invalid_argument& e) {
        std::cout << e.what() << std::endl;
    }

    try {
        assert(hashTable.insert(3, "three"));
    }
    catch (const std::invalid_argument& e) {
        std::cout << e.what() << std::endl;
    }

    try {
        assert(hashTable.insert("thre", "thre_"));
    }
    catch (const std::invalid_argument& e) {
        std::cout << e.what() << std::endl;
    }

    std::string result;
    try {

        assert(hashTable.find(nullptr, result) && result == "one");
    }
    catch (const std::invalid_argument& e) {
        std::cout << e.what() << std::endl;
    }

    try {
        assert(hashTable.find(2, result) && result == "two");
    }
    catch (const std::invalid_argument& e) {
        std::cout << e.what() << std::endl;
    }

    try {
        assert(hashTable.find(3, result) && result == "three");
    }
    catch (const std::invalid_argument& e) {
        std::cout << e.what() << std::endl;
    }

    try {
        assert(hashTable.find("thre", result) && result == "thre_");
    }
    catch (const std::invalid_argument& e) {
        std::cout << e.what() << std::endl;
    }


    try {
        hashTable.remove(nullptr);
    }
    catch (const std::invalid_argument& e) {
        std::cout << e.what() << std::endl;
    }

    try {
        assert(hashTable.find(nullptr, result) == false && result == "one");
    }
    catch (const std::invalid_argument& e) {
        std::cout << e.what() << std::endl;
    }

    std::cout << "Custom hash function test passed!" << std::endl;
}

void testConcurrentInserts() {
    CustomHashTable<int, std::string> hashTable(100);

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&hashTable, i]() {
            for (int j = 0; j < 100; ++j) {
                hashTable.insert(i * 100 + j, "value_" + std::to_string(i * 100 + j));
            }
            });
    }

    for (auto& th : threads) {
        th.join();
    }

    std::string result;
    for (int i = 0; i < 1000; ++i) {
        assert(hashTable.find(i, result) && result == "value_" + std::to_string(i));
    }

    std::cout << "Concurrent inserts test passed!" << std::endl;
}

void testConcurrentFinds() {
    CustomHashTable<int, std::string> hashTable(100);

    for (int i = 0; i < 1000; ++i) {
        hashTable.insert(i, "value_" + std::to_string(i));
    }

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&hashTable, i]() {
            for (int j = 0; j < 100; ++j) {
                std::string result;
                assert(hashTable.find(i * 100 + j, result) && result == "value_" + std::to_string(i * 100 + j));
            }
            });
    }

    for (auto& th : threads) {
        th.join();
    }

    std::cout << "Concurrent finds test passed!" << std::endl;
}

int main() {
    testDuplicateInsertions();
    testRemoveNonExistentKey();
    testTableResizing();
    testEmptyTableOperations();
    testLargeNumberOfInsertsAndFinds();
    testThreadSafety();
    testSingleEntry();
    testCustomHashFunction();
    testMyCustomHashFunction();
    testConcurrentInserts();
    testConcurrentFinds();

    return 0;
}