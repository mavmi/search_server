#pragma once

#include <cassert>
#include <string>
#include <iostream>
#include <vector>


#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))
#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))
#define RUN_TEST(func) RunTestImpl((func), #func)


static constexpr double eps = 1e-6;


void PrintSeparator();


template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
    const std::string& func, unsigned line, const std::string& hint) {
    using namespace std::string_literals;
    if (t != u) {
        std::cout << std::boolalpha;
        std::cout << file << "("s << line << "): "s << func << ": "s;
        std::cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            std::cout << " Hint: "s << hint;
        }
        std::cout << std::endl;
        abort();
    }
}


void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func, unsigned line,
    const std::string& hint);


template <typename Func>
void RunTestImpl(Func func, std::string func_name) {
    using namespace std::string_literals;
    func();
    std::cerr << func_name << " OK"s << std::endl;
    PrintSeparator();
}


void PrintDocument(const Document& document);
void PrintMatchDocumentResult(int document_id, const std::vector<std::string>& words, DocumentStatus status);
void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status,
    const std::vector<int>& ratings);
void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);
void MatchDocuments(const SearchServer& search_server, const std::string& query);


void TestSearchAddedDocument();
void TestExcludeStopWordsFromAddedDocumentContent();
void TestMinusWords();
void TestMatchDocument();
void TestRelevanceSorting();
void TestRating();
void TestPredicat();
void TestStatus();
void TestRelevance();
void TestGetDocumentCount();
void TestRequestQueue();
void TestPaginator();
void TestSearchServerExceptions();
void TestDuplicateRemover();
void TestProcessQueries();
void TestProcessQueriesJoined();
void TestParallelMatchDocument();
void TestParallelFindTopDocuments();
void TestFindTopDocumentsSpeed();
void TestSearchServer();