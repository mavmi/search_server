#include <math.h>

#include "paginator.h"
#include "test_example_functions.h"
#include "request_queue.h"
#include "string_processing.h"
#include "process_queries.h"
#include "remove_duplicates.h"


using namespace std;


void PrintSeparator() {
    cout << endl << endl;
    cout << "****************************"s << endl;
    cout << "****************************"s << endl;
    cout << endl << endl;
}


void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
    const std::string& hint) {
    if (!value) {
        using namespace std::string_literals;
        std::cout << file << "("s << line << "): "s << func << ": "s;
        std::cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            std::cout << " Hint: "s << hint;
        }
        std::cout << std::endl;
        abort();
    }
}


void PrintDocument(const Document& document) {
    using namespace std::string_literals;
    std::cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << std::endl;
}


void PrintMatchDocumentResult(int document_id, const std::vector<std::string_view>& words, DocumentStatus status) {
    using namespace std::string_literals;
    std::cout << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (const std::string_view& word : words) {
        std::cout << ' ' << word;
    }
    std::cout << "}"s << std::endl;
}


void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status,
    const std::vector<int>& ratings) {
    using namespace std::string_literals;
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const std::invalid_argument& e) {
        std::cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << std::endl;
    }
}


void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query) {
    using namespace std::string_literals;
    std::cout << "Результаты поиска по запросу: "s << raw_query << std::endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const std::invalid_argument& e) {
        std::cout << "Ошибка поиска: "s << e.what() << std::endl;
    }
}


void MatchDocuments(const SearchServer& search_server, const std::string& query) {
    using namespace std::string_literals;
    try {
        std::cout << "Матчинг документов по запросу: "s << query << std::endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const std::invalid_argument& e) {
        std::cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << std::endl;
    }
}


void TestSearchAddedDocument() {

    //Searching for a document by words from it
    {
        SearchServer searchServer;
        searchServer.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
        const auto found_documents = searchServer.FindTopDocuments("cat in the city"s);
        ASSERT_EQUAL_HINT(found_documents.size(), 1, "found_documents.size() = 1");
        const Document document = found_documents[0];
        ASSERT_EQUAL_HINT(document.id, 42, "id = 42");
    }

    //Searching for a document using only one word
    {
        SearchServer searchServer;
        searchServer.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
        const auto found_documents = searchServer.FindTopDocuments("cat"s);
        ASSERT_EQUAL_HINT(found_documents.size(), 1, "found_documents.size() = 1");
        const Document document = found_documents[0];
        ASSERT_EQUAL_HINT(document.id, 42, "id = 42");
    }
    {
        SearchServer searchServer;
        searchServer.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
        const auto found_documents = searchServer.FindTopDocuments("city"s);
        ASSERT_EQUAL_HINT(found_documents.size(), 1, "found_documents.size() = 1");
        const Document document = found_documents[0];
        ASSERT_EQUAL_HINT(document.id, 42, "id = 42");
    }

    //Searching for a document using couple of words
    {
        SearchServer searchServer;
        searchServer.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
        const auto found_documents = searchServer.FindTopDocuments("city cat the"s);
        ASSERT_EQUAL_HINT(found_documents.size(), 1, "found_documents.size() = 1");
        const Document document = found_documents[0];
        ASSERT_EQUAL_HINT(document.id, 42, "id = 42");
    }

    //Check that document we found has correct document status
    {
        SearchServer searchServer;
        searchServer.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
        searchServer.AddDocument(43, "cat in the big city"s, DocumentStatus::IRRELEVANT, { 1, 2, 3 });
        auto found_documents = searchServer.FindTopDocuments("city cat"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL_HINT(found_documents.size(), 1, "found_documents.size() = 1");
        ASSERT_EQUAL_HINT(found_documents[0].id, 42, "id = 42");
        found_documents = searchServer.FindTopDocuments("city cat"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL_HINT(found_documents.size(), 1, "found_documents.size() = 1");
        ASSERT_EQUAL_HINT(found_documents[0].id, 43, "id = 43");
    }

    //Add some documents and check that we can find them
    {
        SearchServer searchServer;
        searchServer.AddDocument(1, "cat", DocumentStatus::ACTUAL, { 1, 2, 3 });
        searchServer.AddDocument(2, "cat and dog", DocumentStatus::ACTUAL, { 1, 2, 3 });
        searchServer.AddDocument(3, "fat cat", DocumentStatus::ACTUAL, { 1, 2, 3 });
        searchServer.AddDocument(4, "cat in the city", DocumentStatus::ACTUAL, { 1, 2, 3 });
        searchServer.AddDocument(5, "sun", DocumentStatus::ACTUAL, { 1, 2, 3 });

        auto found_docs = searchServer.FindTopDocuments("cat");
        for (const Document& doc : found_docs) {
            ASSERT(doc.id == 1 || doc.id == 2 || doc.id == 3 || doc.id == 4);
        }
    }

    //Check that there are no wrong documents
    {
        SearchServer searchServer;
        searchServer.AddDocument(1, "cat", DocumentStatus::ACTUAL, { 1, 2, 3 });
        searchServer.AddDocument(2, "cat and dog", DocumentStatus::ACTUAL, { 1, 2, 3 });
        searchServer.AddDocument(3, "fat cat", DocumentStatus::ACTUAL, { 1, 2, 3 });
        searchServer.AddDocument(4, "cat in the city", DocumentStatus::ACTUAL, { 1, 2, 3 });
        searchServer.AddDocument(5, "sun", DocumentStatus::ACTUAL, { 1, 2, 3 });

        auto found_docs = searchServer.FindTopDocuments("yellow");
        ASSERT_HINT(found_docs.empty(), "found_docs is empty");
    }

    //Check that nothing works if we don't add documents
    {
        SearchServer searchServer;
        auto found_docs = searchServer.FindTopDocuments("yellow");
        ASSERT_HINT(found_docs.empty(), "found_docs is empty");
    }

    //Check the result using empty query
    {
        SearchServer searchServer;
        searchServer.AddDocument(1, "cat", DocumentStatus::ACTUAL, { 1, 2, 3 });
        searchServer.AddDocument(2, "cat and dog", DocumentStatus::ACTUAL, { 1, 2, 3 });
        searchServer.AddDocument(3, "fat cat", DocumentStatus::ACTUAL, { 1, 2, 3 });
        searchServer.AddDocument(4, "cat in the city", DocumentStatus::ACTUAL, { 1, 2, 3 });
        searchServer.AddDocument(5, "sun", DocumentStatus::ACTUAL, { 1, 2, 3 });

        auto found_docs = searchServer.FindTopDocuments("");
        ASSERT_HINT(found_docs.empty(), "found_docs is empty");
    }
}


void TestExcludeStopWordsFromAddedDocumentContent() {
    constexpr int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    // Check that search for a word which is not in stop-words list finds correct document
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1, "found_docs.size() = 1");
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    //Check that search for a word from stop-words list returns empty result
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}


void TestMinusWords() {
    constexpr int doc_id = 0;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    constexpr int doc_id_1 = 1;
    const string content_1 = "welcome to the rice field"s;
    const vector<int> ratings_1 = { 11, 12, 13 };

    //"in" is minus-word
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        const auto found_docs = server.FindTopDocuments("the -in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id_1);
    }

    //"the" is minus-word
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        const auto found_docs = server.FindTopDocuments("cat -the"s);
        ASSERT_EQUAL(found_docs.size(), 0);
    }

    //"the" is minus-word and documents have different statuses
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::IRRELEVANT, ratings_1);
        const auto found_docs = server.FindTopDocuments("cat -the"s);
        ASSERT_EQUAL(found_docs.size(), 0);
    }
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::IRRELEVANT, ratings_1);
        const auto found_docs = server.FindTopDocuments("cat -the"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_docs.size(), 0);
    }
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(123, "the big tower of cat", DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("-the cat"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(found_docs.size(), 0);
    }
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(123, "the big tower", DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("the -cat"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(found_docs.size(), 2);
    }
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(123, "the big tower", DocumentStatus::IRRELEVANT, ratings);
        const auto found_docs = server.FindTopDocuments("the -cat"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_docs.size(), 1);
    }
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::IRRELEVANT, ratings_1);
        server.AddDocument(123, "the big tower of cat", DocumentStatus::REMOVED, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(found_docs.size(), 0);
    }

    //Add different minus-words
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(123, "the big tower of cat", DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("-word the"s, DocumentStatus::ACTUAL);
        ASSERT(found_docs.size() == 3);
    }

    //"a" is minus-word
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        const auto found_docs = server.FindTopDocuments("the -a"s);
        ASSERT(found_docs.size() == 2);
    }

    //Some word will be plus-word and minus-word at the same time
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(123, "the big tower of cat", DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("-the"s, DocumentStatus::ACTUAL);
        ASSERT(found_docs.size() == 0);
    }

    //"a" and "cat" are minus-words
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        const auto found_docs = server.FindTopDocuments("the -a -cat"s);
        ASSERT(found_docs.size() == 1);
        ASSERT(found_docs[0].id == 1);
    }
}


void TestMatchDocument() {
    constexpr int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<string> content_vector = SplitIntoWords(content);
    const vector<int> ratings = { 1, 2, 3 };

    // Check that query without minus-words will return all words from query that are in the document
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        string query = "cat in the"s;
        const auto [plus_words, status] = server.MatchDocument(query, doc_id);

        bool flag = true;
        ASSERT_HINT(plus_words.size() == 3, "plus_words.size() = 3");
        for (int i = 0; i < plus_words.size(); ++i) {
            for (int j = 0; j < content_vector.size(); ++j) {
                if (plus_words[i] == content_vector[j]) {
                    break;
                }
                if (j == content_vector.size() - 1 && plus_words[i] != content_vector[j]) {
                    flag = false;
                }
            }
            if (!flag) {
                break;
            }
        }
        ASSERT(flag == true);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        string query = "cat in the god damn city"s;
        const auto [plus_words, status] = server.MatchDocument(query, doc_id);
        bool flag = true;
        ASSERT_HINT(plus_words.size() == 4, "plus_words.size() = 4");
        for (int i = 0; i < plus_words.size(); ++i) {
            for (int j = 0; j < content_vector.size(); ++j) {
                if (plus_words[i] == content_vector[j]) {
                    break;
                }
                if (j == content_vector.size() - 1 && plus_words[i] != content_vector[j]) {
                    flag = false;
                }
            }
            if (!flag) {
                break;
            }
        }
        ASSERT(flag == true);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        string query = "cat in the city"s;
        const auto [plus_words, status] = server.MatchDocument(query, doc_id);
        bool flag = true;
        ASSERT_HINT(plus_words.size() == 4, "plus_words.size() = 4");
        for (int i = 0; i < plus_words.size(); ++i) {
            for (int j = 0; j < content_vector.size(); ++j) {
                if (plus_words[i] == content_vector[j]) {
                    break;
                }
                if (j == content_vector.size() - 1 && plus_words[i] != content_vector[j]) {
                    flag = false;
                }
            }
            if (!flag) {
                break;
            }
        }
        ASSERT(flag == true);
    }

    //Check the result with minus-words in the query
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        string query = "cat in -the city"s;
        vector<string> query_vector = SplitIntoWords(query);
        const auto [plus_words, status] = server.MatchDocument(query, doc_id);
        ASSERT_EQUAL(plus_words.size(), 0);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        string query = "-the"s;
        vector<string> query_vector = SplitIntoWords(query);
        const auto [plus_words, status] = server.MatchDocument(query, doc_id);
        ASSERT_EQUAL(plus_words.size(), 0);
    }

    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        string query = "cat in -the -city"s;
        vector<string> query_vector = SplitIntoWords(query);
        const auto [plus_words, status] = server.MatchDocument(query, doc_id);
        ASSERT_EQUAL(plus_words.size(), 0);
    }

    //Check the result with a different minus-words in the query which are not from the document
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        string query = "cat in the city -apple"s;
        vector<string> query_vector = SplitIntoWords(query);
        const auto [plus_words, status] = server.MatchDocument(query, doc_id);
        ASSERT_EQUAL(plus_words.size(), 4);
    }

    //Check if server returns documents with correct statuses
    {
        SearchServer server;
        server.AddDocument(1, "aa aaaa", DocumentStatus::ACTUAL, ratings);
        server.AddDocument(2, "bb bbbb", DocumentStatus::BANNED, ratings);
        server.AddDocument(3, "cc xxxx", DocumentStatus::IRRELEVANT, ratings);
        server.AddDocument(4, "dd awsrgfrew", DocumentStatus::REMOVED, ratings);

        auto docs = server.MatchDocument("aa", 1);
        DocumentStatus status = get<DocumentStatus>(docs);
        ASSERT(status == DocumentStatus::ACTUAL);
        ASSERT(status != DocumentStatus::BANNED);
        ASSERT(status != DocumentStatus::IRRELEVANT);
        ASSERT(status != DocumentStatus::REMOVED);

        docs = server.MatchDocument("bb", 2);
        status = get<DocumentStatus>(docs);
        ASSERT_HINT(status != DocumentStatus::ACTUAL, "status != DocumentStatus::ACTUAL");
        ASSERT_HINT(status == DocumentStatus::BANNED, "status == DocumentStatus::BANNED");
        ASSERT_HINT(status != DocumentStatus::IRRELEVANT, "status != DocumentStatus::IRRELEVANT");
        ASSERT_HINT(status != DocumentStatus::REMOVED, "status != DocumentStatus::REMOVED");

        docs = server.MatchDocument("cc", 3);
        status = get<DocumentStatus>(docs);
        ASSERT(status != DocumentStatus::ACTUAL);
        ASSERT(status != DocumentStatus::BANNED);
        ASSERT(status == DocumentStatus::IRRELEVANT);
        ASSERT(status != DocumentStatus::REMOVED);

        docs = server.MatchDocument("d", 4);
        status = get<DocumentStatus>(docs);
        ASSERT_HINT(status != DocumentStatus::ACTUAL, "status != DocumentStatus::ACTUAL");
        ASSERT_HINT(status != DocumentStatus::BANNED, "status != DocumentStatus::BANNED");
        ASSERT_HINT(status != DocumentStatus::IRRELEVANT, "status != DocumentStatus::IRRELEVANT");
        ASSERT_HINT(status == DocumentStatus::REMOVED, "status == DocumentStatus::REMOVED");
    }
}


void TestRelevanceSorting() {

    //Check sorting of results when all documents have ACTUAL status
    {
        SearchServer server;

        constexpr int doc_id = 0;
        const string content = "cat in the"s;
        const vector<int> ratings = { 1, 2, 3 };
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        constexpr int doc_id_2 = 1;
        const string content_2 = "cat in the city"s;
        const vector<int> ratings_2 = { 11, 12, 13 };
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);

        constexpr int doc_id_3 = 2;
        const string content_3 = "cat in the god damn city ohhhh"s;
        const vector<int> ratings_3 = { 21, 22, 23 };
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);

        constexpr int doc_id_4 = 3;
        const string content_4 = "welcome to the rice fields"s;
        const vector<int> ratings_4 = { 31, 32, 33 };
        server.AddDocument(doc_id_4, content_4, DocumentStatus::ACTUAL, ratings_4);

        constexpr int doc_id_5 = 4;
        const string content_5 = "aa"s;
        const vector<int> ratings_5 = { 31, 32, 33 };
        server.AddDocument(doc_id_5, content_5, DocumentStatus::ACTUAL, ratings_5);

        constexpr int doc_id_6 = 5;
        const string content_6 = "cat in the city"s;
        const vector<int> ratings_6 = { 41, 42, 43 };
        server.AddDocument(doc_id_6, content_6, DocumentStatus::ACTUAL, ratings_6);

        vector<Document> docs = server.FindTopDocuments("cat in the city");
        for (int i = 0; i < docs.size() - 1; ++i) {
            ASSERT(docs[i].relevance >= docs[i + 1].relevance);
        }
        for (int i = 0; i < docs.size() - 1; ++i) {
            ASSERT(docs[i].relevance > docs[i + 1].relevance || docs[i].relevance == docs[i + 1].relevance);
        }
        for (int i = 0; i < docs.size() - 1; ++i) {
            ASSERT((docs[i].relevance - docs[i + 1].relevance < eps&& docs[i].rating >= docs[i + 1].rating) || docs[i].relevance - docs[i + 1].relevance > eps);
        }
    }

    //Check sorting of results when all documents have different statuses
    {
        SearchServer server;

        constexpr int doc_id = 0;
        const string content = "cat in the"s;
        const vector<int> ratings = { 1, 2, 3 };
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        constexpr int doc_id_2 = 1;
        const string content_2 = "cat in the city"s;
        const vector<int> ratings_2 = { 11, 12, 13 };
        server.AddDocument(doc_id_2, content_2, DocumentStatus::IRRELEVANT, ratings_2);

        constexpr int doc_id_3 = 2;
        const string content_3 = "cat in the god damn city man"s;
        const vector<int> ratings_3 = { 21, 22, 23 };
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);

        constexpr int doc_id_4 = 3;
        const string content_4 = "welcome to the rice fields"s;
        const vector<int> ratings_4 = { 31, 32, 33 };
        server.AddDocument(doc_id_4, content_4, DocumentStatus::BANNED, ratings_4);

        constexpr int doc_id_5 = 4;
        const string content_5 = "aa"s;
        const vector<int> ratings_5 = { 31, 32, 33 };
        server.AddDocument(doc_id_5, content_5, DocumentStatus::ACTUAL, ratings_5);

        constexpr int doc_id_6 = 5;
        const string content_6 = "cat in the city"s;
        const vector<int> ratings_6 = { 41, 42, 43 };
        server.AddDocument(doc_id_6, content_6, DocumentStatus::ACTUAL, ratings_6);

        vector<Document> docs = server.FindTopDocuments("cat in the city", DocumentStatus::ACTUAL);
        for (int i = 0; i < docs.size() - 1; ++i) {
            ASSERT_HINT(docs[i].relevance >= docs[i + 1].relevance, "documents are sorted by relevance in non-increasing order");
        }
    }
}


void TestRating() {

    //Compare document's rating with the expected one
    {
        SearchServer server;
        server.AddDocument(42, "cat in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT(found_docs.size() == 1);
        const Document& doc0 = found_docs[0];
        ASSERT(doc0.rating == 2);
    }

    //Check that ratings are calculating correctly and documents are sorted in non-increasing order
    {
        SearchServer server;
        server.AddDocument(45, "cat in the city"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
        server.AddDocument(23, "big fat cat"s, DocumentStatus::ACTUAL, { 0 });
        server.AddDocument(0, "cool cat does dab"s, DocumentStatus::ACTUAL, { 4, 5, 6 });
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT(found_docs.size() == 3);
        for (int i = 0; i < found_docs.size() - 1; ++i) {
            ASSERT_HINT(found_docs[i].rating >= found_docs[i + 1].rating, "rating of the next document can not be higher that rating of the previous one");
        }
        ASSERT(found_docs[0].rating == (4 + 5 + 6) / 3);
        ASSERT(found_docs[1].rating == (1 + 2 + 3) / 3);
        ASSERT(found_docs[2].rating == 0);
    }

    //Do the same with a negative rating
    {
        SearchServer server;
        server.AddDocument(45, "cat in the city"s, DocumentStatus::ACTUAL, { -3 });
        server.AddDocument(23, "big fat cat"s, DocumentStatus::ACTUAL, { 0 });
        server.AddDocument(0, "cool cat does dab"s, DocumentStatus::ACTUAL, { 4, 5, 6 });
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs.size(), 3);
        for (int i = 0; i < found_docs.size() - 1; ++i) {
            ASSERT_HINT(found_docs[i].rating >= found_docs[i + 1].rating, "rating of the next document can not be higher that rating of the previous one");
        }
        ASSERT_EQUAL(found_docs[0].rating, (4 + 5 + 6) / 3);
        ASSERT_EQUAL(found_docs[1].rating, 0);
        ASSERT_EQUAL(found_docs[2].rating, -3);
    }

    //Eliminate relevance by making content of documents the same
    {
        SearchServer server;
        server.AddDocument(45, "cat in the city"s, DocumentStatus::ACTUAL, { -3 });
        server.AddDocument(23, "cat in the city"s, DocumentStatus::ACTUAL, { 0 });
        server.AddDocument(0, "cat in the city"s, DocumentStatus::ACTUAL, { 4, 5, 6 });
        server.AddDocument(3, "cat in the city"s, DocumentStatus::ACTUAL, { -10, -3, -3 });
        const auto found_docs = server.FindTopDocuments("cat"s);
        ASSERT_EQUAL(found_docs.size(), 4);
        for (int i = 0; i < found_docs.size() - 1; ++i) {
            ASSERT_HINT(found_docs[i].rating >= found_docs[i + 1].rating, "rating of the next document can not be higher that rating of the previous one");
        }
        ASSERT_EQUAL(found_docs[0].rating, (4 + 5 + 6) / 3);
        ASSERT_EQUAL(found_docs[1].rating, 0);
        ASSERT_EQUAL(found_docs[2].rating, -3);
        ASSERT_EQUAL(found_docs[3].rating, (-10 - 3 - 4) / 3);
    }
}


void TestPredicat() {

    //Chech the perfomance of predicate function with variables at the input
    {
        SearchServer server;

        constexpr int doc_id = 0;
        const string content = "cat in the"s;
        const vector<int> ratings = { 1, 2, 3 };
        server.AddDocument(doc_id, content, DocumentStatus::BANNED, ratings);

        constexpr int doc_id_2 = 1;
        const string content_2 = "cat in the city"s;
        const vector<int> ratings_2 = { 11, 12, 13 };
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);

        constexpr int doc_id_3 = 2;
        const string content_3 = "cat in the god damn city man"s;
        const vector<int> ratings_3 = { 21, 22, 23 };
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);

        constexpr int doc_id_4 = 3;
        const string content_4 = "the cat"s;
        const vector<int> ratings_4 = { 31, 32, 33 };
        server.AddDocument(doc_id_4, content_4, DocumentStatus::ACTUAL, ratings_4);

        constexpr int doc_id_5 = 4;
        const string content_5 = "aa"s;
        const vector<int> ratings_5 = { 31, 32, 33 };
        server.AddDocument(doc_id_5, content_5, DocumentStatus::ACTUAL, ratings_5);

        constexpr int doc_id_6 = 5;
        const string content_6 = "cat in the city"s;
        const vector<int> ratings_6 = { 41, 42, 43 };
        server.AddDocument(doc_id_6, content_6, DocumentStatus::ACTUAL, ratings_6);

        vector<Document> docs = server.FindTopDocuments("cat in the city", []([[maybe_unused]] int id, [[maybe_unused]] DocumentStatus status, [[maybe_unused]] int rating) {
            return id % 2 == 0;
            });
        ASSERT(docs.size() == 2);
        for (const Document& doc : docs) {
            ASSERT(doc.id == 0 || doc.id == 2);
        }

        docs = server.FindTopDocuments("cat in the city", []([[maybe_unused]] int id, [[maybe_unused]] DocumentStatus status, [[maybe_unused]] int rating) {
            return rating > 10;
            });
        ASSERT(docs.size() == 4);

        docs = server.FindTopDocuments("cat in the city", []([[maybe_unused]] int id, [[maybe_unused]] DocumentStatus status, [[maybe_unused]] int rating) {
            return status == DocumentStatus::BANNED;
            });
        ASSERT(docs.size() == 1);
        ASSERT(docs[0].id == doc_id);
    }
}


void TestStatus() {

    //Check the search for documents by status when documents have different statuses and ratings
    {
        SearchServer server;

        constexpr int doc_id = 0;
        const string content = "cat in the"s;
        const vector<int> ratings = { 1, 2, 3 };
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        constexpr int doc_id_2 = 1;
        const string content_2 = "cat in the city"s;
        const vector<int> ratings_2 = { 11, 12, 13 };
        server.AddDocument(doc_id_2, content_2, DocumentStatus::IRRELEVANT, ratings_2);

        constexpr int doc_id_3 = 2;
        const string content_3 = "cat in the god damn city man"s;
        const vector<int> ratings_3 = { 21, 22, 23 };
        server.AddDocument(doc_id_3, content_3, DocumentStatus::IRRELEVANT, ratings_3);

        constexpr int doc_id_4 = 3;
        const string content_4 = "welcome to the rice fields"s;
        const vector<int> ratings_4 = { 31, 32, 33 };
        server.AddDocument(doc_id_4, content_4, DocumentStatus::IRRELEVANT, ratings_4);

        constexpr int doc_id_5 = 4;
        const string content_5 = "aa"s;
        const vector<int> ratings_5 = { 31, 32, 33 };
        server.AddDocument(doc_id_5, content_5, DocumentStatus::BANNED, ratings_5);

        constexpr int doc_id_6 = 5;
        const string content_6 = "cat in the city"s;
        const vector<int> ratings_6 = { 41, 42, 43 };
        server.AddDocument(doc_id_6, content_6, DocumentStatus::BANNED, ratings_6);

        auto found_documents = server.FindTopDocuments("cat in");
        ASSERT(found_documents.size() == 1);
        ASSERT(found_documents[0].id == 0);

        found_documents = server.FindTopDocuments("cat in", DocumentStatus::ACTUAL);
        ASSERT(found_documents.size() == 1);
        ASSERT(found_documents[0].id == 0);

        found_documents = server.FindTopDocuments("cat in", DocumentStatus::REMOVED);
        ASSERT(found_documents.size() == 0);

        found_documents = server.FindTopDocuments("cat in", DocumentStatus::IRRELEVANT);
        ASSERT(found_documents.size() == 2);

        found_documents = server.FindTopDocuments("the", DocumentStatus::IRRELEVANT);
        ASSERT(found_documents.size() == 3);
        for (const Document& doc : found_documents) {
            ASSERT(doc.id == 1 || doc.id == 2 || doc.id == 3);
        }
    }

    //Do the same with documents with the same ratings
    {
        SearchServer server;

        constexpr int doc_id = 0;
        const string content = "cat in the"s;
        const vector<int> ratings = { 1, 2, 3 };
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        constexpr int doc_id_2 = 1;
        const string content_2 = "cat in the city"s;
        const vector<int> ratings_2 = { 11, 12, 13 };
        server.AddDocument(doc_id_2, content_2, DocumentStatus::IRRELEVANT, ratings);

        constexpr int doc_id_3 = 2;
        const string content_3 = "cat in the god damn city man"s;
        const vector<int> ratings_3 = { 21, 22, 23 };
        server.AddDocument(doc_id_3, content_3, DocumentStatus::IRRELEVANT, ratings);

        constexpr int doc_id_4 = 3;
        const string content_4 = "welcome to the rice fields"s;
        const vector<int> ratings_4 = { 31, 32, 33 };
        server.AddDocument(doc_id_4, content_4, DocumentStatus::IRRELEVANT, ratings);

        constexpr int doc_id_5 = 4;
        const string content_5 = "aa"s;
        const vector<int> ratings_5 = { 31, 32, 33 };
        server.AddDocument(doc_id_5, content_5, DocumentStatus::BANNED, ratings);

        constexpr int doc_id_6 = 5;
        const string content_6 = "cat in the city"s;
        const vector<int> ratings_6 = { 41, 42, 43 };
        server.AddDocument(doc_id_6, content_6, DocumentStatus::BANNED, ratings);

        auto found_documents = server.FindTopDocuments("cat in");
        ASSERT_EQUAL(found_documents.size(), 1);
        ASSERT_EQUAL(found_documents[0].id, 0);

        found_documents = server.FindTopDocuments("cat in", DocumentStatus::ACTUAL);
        ASSERT_EQUAL(found_documents.size(), 1);
        ASSERT_EQUAL(found_documents[0].id, 0);

        found_documents = server.FindTopDocuments("cat in", DocumentStatus::REMOVED);
        ASSERT_EQUAL(found_documents.size(), 0);

        found_documents = server.FindTopDocuments("cat in", DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_documents.size(), 2);

        found_documents = server.FindTopDocuments("the", DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_documents.size(), 3);
        for (const Document& doc : found_documents) {
            ASSERT(doc.id == 1 || doc.id == 2 || doc.id == 3);
        }
    }
}


void TestRelevance() {

    //Compare document's relevance with expected one using specified error
    {
        SearchServer server;

        server.AddDocument(0, "cat"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
        server.AddDocument(1, "cat chair tree"s, DocumentStatus::ACTUAL, { 11, 12, 13 });
        server.AddDocument(2, "yellow sun"s, DocumentStatus::ACTUAL, { 21, 22, 23 });

        double IDF_cat = log(3.0 / 2.0);
        double IDF_chair = log(3.0 / 1.0);

        double TF_cat_0 = 1.0;
        double TF_cat_1 = 1.0 / 3.0;
        double TF_chair_1 = 1.0 / 3.0;

        double relevance_0 = IDF_cat * TF_cat_0;
        double relevance_1 = IDF_cat * TF_cat_1 + IDF_chair * TF_chair_1;

        const auto found_documents = server.FindTopDocuments("cat chair"s);

        ASSERT(found_documents[1].relevance - relevance_0 < eps);
        ASSERT(found_documents[0].relevance - relevance_1 < eps);

        ASSERT(found_documents[1].relevance == relevance_0);
        ASSERT(found_documents[0].relevance == relevance_1);
    }
}


void TestGetDocumentCount() {
    constexpr int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    constexpr int doc_id_1 = 43;
    const string content_1 = "cat in this city"s;
    const vector<int> ratings_1 = { 11, 22, 33 };

    //Search for a document by words from it
    {
        SearchServer searchServer;
        searchServer.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        searchServer.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        const auto found_documents = searchServer.FindTopDocuments("cat"s);
        ASSERT(found_documents.size() == 2);
        ASSERT(searchServer.GetDocumentCount() == 2);
    }
}


void TestRequestQueue() {
    SearchServer search_server("и в на"s);
    RequestQueue request_queue(search_server);

    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    search_server.AddDocument(3, "большой кот модный ошейник "s, DocumentStatus::ACTUAL, { 1, 2, 8 });
    search_server.AddDocument(4, "большой пёс скворец евгений"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
    search_server.AddDocument(5, "большой пёс скворец василий"s, DocumentStatus::ACTUAL, { 1, 1, 1 });

    constexpr int empty_requests = 1439;
    for (int i = 0; i < empty_requests; ++i) {
        request_queue.AddFindRequest("пустой запрос"s);
    }
    string hint = to_string(empty_requests) + " requests with empty result"s;
    ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), empty_requests, hint);

    hint = "still "s + to_string(empty_requests) + " requests with empty result"s;
    request_queue.AddFindRequest("пушистый пёс"s);
    ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), empty_requests, hint);

    hint = "new day, first request was deleted, "s + to_string(empty_requests - 1) + " requests with empty result"s;
    request_queue.AddFindRequest("большой ошейник"s);
    ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), empty_requests - 1, hint);

    hint = "first request was deleted, "s + to_string(empty_requests - 2) + " requests with empty result"s;
    request_queue.AddFindRequest("скворец"s);
    ASSERT_EQUAL_HINT(request_queue.GetNoResultRequests(), empty_requests - 2, hint);
}


void TestPaginator() {
    SearchServer search_server("and with"s);

    search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    search_server.AddDocument(3, "big cat nasty hair"s, DocumentStatus::ACTUAL, { 1, 2, 8 });
    search_server.AddDocument(4, "big dog cat Vladislav"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
    search_server.AddDocument(5, "big dog hamster Borya"s, DocumentStatus::ACTUAL, { 1, 1, 1 });

    const auto search_results = search_server.FindTopDocuments("curly dog"s);
    int page_size = 2;
    const auto pages = Paginate(search_results, page_size);

    ASSERT_EQUAL(distance(pages.begin(), pages.end()), page_size);

    for (auto page = pages.begin(); page != pages.end(); ++page) {
        //first page
        if (distance((*page).begin(), (*page).end()) == 2) {
            Document doc1 = *(*page).begin();
            ASSERT_EQUAL(doc1.id, 2);
            ASSERT_EQUAL(doc1.rating, 2);

            Document doc2 = *(((*page).begin()) + 1);
            ASSERT_EQUAL(doc2.id, 4);
            ASSERT_EQUAL(doc2.rating, 2);
            cout << "Print document: "s;
            PrintDocument(doc2);
        }
        //second page
        else {
            Document doc1 = *(*page).begin();
            ASSERT_EQUAL(doc1.id, 5);
            ASSERT_EQUAL(doc1.rating, 1);
        }
        cout << *page << endl;
        cout << "Page break"s << endl;
    }
}


void TestSearchServerExceptions() {
    using namespace std::string_literals;
    SearchServer search_server("и в на"s);
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    try {
        search_server.AddDocument(1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
        abort();
    } catch (std::invalid_argument& e) {
        std::cout << "Cathced error: "s << e.what() << std::endl;
    }
    try {
        search_server.AddDocument(-1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
        abort();
    } catch (std::invalid_argument& e) {
        std::cout << "Cathced error: "s << e.what() << std::endl;
    }
    try {
        search_server.AddDocument(3, "большой пёс скво\x12рец евгений"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
        abort();
    } catch (std::invalid_argument& e) {
        std::cout << "Cathced error: "s << e.what() << std::endl;
    }
    try {
        search_server.FindTopDocuments("пушистый --кот"s);
        abort();
    } catch (std::invalid_argument& e) {
        std::cout << "Cathced error: "s << e.what() << std::endl;
    }
    try {
        search_server.FindTopDocuments("пушистый -"s);
        abort();
    } catch (std::invalid_argument& e) {
        std::cout << "Cathced error: "s << e.what() << std::endl;
    }
    try {
        search_server.MatchDocument("модный --пёс"s, 0);
        abort();
    } catch (std::invalid_argument& e) {
        std::cout << "Cathced error: "s << e.what() << std::endl;
    }
    FindTopDocuments(search_server, "модный --пёс"s);
}


void TestDuplicateRemover() {
    SearchServer search_server("and with"s);

    AddDocument(search_server, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    AddDocument(search_server, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // duplicate of document #2, must be deleted
    AddDocument(search_server, 3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // difference is only in stop-words, it is duplicate
    AddDocument(search_server, 4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // set of words is the same, it is duplicate of document #1
    AddDocument(search_server, 5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // there are new words, it is not duplicate
    AddDocument(search_server, 6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // the set of words is the same as in document which id is 6, 
    // despite the different order, it is duplicate
    AddDocument(search_server, 7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, { 1, 2 });

    // there are not all the words, it is not duplicate
    AddDocument(search_server, 8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // words are from different documents, it is not duplicate
    AddDocument(search_server, 9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    ASSERT_EQUAL(search_server.GetDocumentCount(), 9);
    RemoveDuplicates(search_server);
    ASSERT_EQUAL(search_server.GetDocumentCount(), 5);
}


void TestProcessQueries() {
    SearchServer search_server("and with"s);

    search_server.AddDocument(0, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });
    search_server.AddDocument(1, "funny pet with curly hair", DocumentStatus::ACTUAL, { 1, 2 });
    search_server.AddDocument(2, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });
    search_server.AddDocument(3, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, { 1, 2 });
    search_server.AddDocument(4, "nasty rat with curly hair", DocumentStatus::ACTUAL, { 1, 2 });

    const vector<string> queries = {
        "nasty rat -not"s,
        "not very funny nasty pet"s,
        "curly hair"s
    };

    int id = 0;

    for (
        const auto& documents : ProcessQueries(search_server, queries)
        ) {
        cout << documents.size() << " documents for query ["s << queries[id++] << "]"s << endl;
    }
}


void TestProcessQueriesJoined() {
    SearchServer search_server("and with"s);

    search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });
    search_server.AddDocument(2, "funny pet with curly hair", DocumentStatus::ACTUAL, { 1, 2 });
    search_server.AddDocument(3, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });
    search_server.AddDocument(4, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, { 1, 2 });
    search_server.AddDocument(5, "nasty rat with curly hair", DocumentStatus::ACTUAL, { 1, 2 });

    const vector<string> queries = {
        "nasty rat -not"s,
        "not very funny nasty pet"s,
        "curly hair"s
    };

    for (const Document& document : ProcessQueriesJoined(search_server, queries)) {
        cout << "Document "s << document.id << " matched with relevance "s << document.relevance << endl;
    }
}


void TestParallelMatchDocument() {
    SearchServer search_server("and with"s);

    search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });
    search_server.AddDocument(2, "funny pet with curly hair", DocumentStatus::ACTUAL, { 1, 2 });
    search_server.AddDocument(3, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });
    search_server.AddDocument(4, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, { 1, 2 });
    search_server.AddDocument(5, "nasty rat with curly hair", DocumentStatus::ACTUAL, { 1, 2 });

    const string query = "curly and funny -not"s;

    {
        const auto [words, status] = search_server.MatchDocument(query, 1);
        cout << words.size() << " words for document 1"s << endl;
        // 1 words for document 1
    }

    {
        const auto [words, status] = search_server.MatchDocument(execution::seq, query, 2);
        cout << words.size() << " words for document 2"s << endl;
        // 2 words for document 2
    }

    {
        const auto [words, status] = search_server.MatchDocument(execution::par, query, 3);
        cout << words.size() << " words for document 3"s << endl;
        // 0 words for document 3
    }
}


void TestParallelFindTopDocuments() {
    SearchServer search_server("and with"s);

    search_server.AddDocument(1, "white cat and yellow hat"s, DocumentStatus::ACTUAL, { 1, 2 });
    search_server.AddDocument(2, "curly cat curly tail"s, DocumentStatus::ACTUAL, { 1, 2 });
    search_server.AddDocument(3, "nasty dog with big eyes"s, DocumentStatus::ACTUAL, { 1, 2 });
    search_server.AddDocument(4, "nasty pigeon john"s, DocumentStatus::ACTUAL, { 1, 2 });

    cout << "ACTUAL by default:"s << endl;
    // Consistent version
    for (const Document& document : search_server.FindTopDocuments("curly nasty cat"s)) {
        PrintDocument(document);
    }
    cout << "BANNED:"s << endl;
    // Consistent version
    for (const Document& document : search_server.FindTopDocuments(execution::seq, "curly nasty cat"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    // Parallel version
    for (const Document& document : search_server.FindTopDocuments(execution::par, "curly nasty cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }
}


void TestFindTopDocumentsSpeed() {
    SearchServer search_server("and with"s);

    constexpr int count_of_documents = 100'000;
    for (int id = 1; id < count_of_documents; id += 4) {
        search_server.AddDocument(id, "white cat and yellow hat"s, DocumentStatus::ACTUAL, { 1, 2 });
        search_server.AddDocument(id + 1, "curly cat curly tail"s, DocumentStatus::ACTUAL, { 1, 2 });
        search_server.AddDocument(id + 2, "nasty dog with big eyes"s, DocumentStatus::ACTUAL, { 1, 2 });
        search_server.AddDocument(id + 3, "nasty pigeon john"s, DocumentStatus::ACTUAL, { 1, 2 });
    }

    // Consistent version
    for (const Document& document : search_server.FindTopDocuments("curly nasty cat"s)) {
        PrintDocument(document);
    }
    // Parallel version
    for (const Document& document : search_server.FindTopDocuments(execution::par, "curly nasty cat"s)) {
        PrintDocument(document);
    }
}


void TestSearchServer() {
    RUN_TEST(TestSearchAddedDocument);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestRelevanceSorting);
    RUN_TEST(TestRating);
    RUN_TEST(TestPredicat);
    RUN_TEST(TestStatus);
    RUN_TEST(TestRelevance);
    RUN_TEST(TestGetDocumentCount);
    RUN_TEST(TestRequestQueue);
    RUN_TEST(TestPaginator);
    RUN_TEST(TestSearchServerExceptions);
    RUN_TEST(TestDuplicateRemover);
    RUN_TEST(TestProcessQueries);
    RUN_TEST(TestProcessQueriesJoined);
    RUN_TEST(TestParallelMatchDocument);
    RUN_TEST(TestParallelFindTopDocuments);
    RUN_TEST(TestFindTopDocumentsSpeed);
}