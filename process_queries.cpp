#include <algorithm>
#include <execution>

#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {

    using namespace std;

    vector<vector<Document>> result(queries.size());
    transform(execution::par, queries.begin(), queries.end(), result.begin(), [&search_server](const string& query) {
        return search_server.FindTopDocuments(query);
        });

    return result;
}

std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {

    using namespace std;

    vector<vector<Document>> all_documents = ProcessQueries(search_server, queries);
    vector<Document> result;
    for (size_t i = 0; i < all_documents.size(); ++i) {
        for (size_t j = 0; j < all_documents[i].size(); ++j) {
            result.push_back(all_documents[i][j]);
        }
    }
    return result;
}