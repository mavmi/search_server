#pragma once

#include <vector>
#include <string>
#include <deque>

#include "search_server.h"


class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        //increase time value and delete old requests
        seconds++;
        while (requests_.size() >= sec_in_day_) {
            if (requests_.front().isEmpty) {
                noResultRequests_--;
            }
            requests_.pop_front();
        }

        //find search results and save them into queue
        QueryResult queryResult;
        queryResult.query_result = search_server_.FindTopDocuments(raw_query, document_predicate);
        queryResult.isEmpty = false;
        if (queryResult.query_result.empty()) {
            queryResult.isEmpty = true;
            noResultRequests_++;
        }
        requests_.push_back(queryResult);
        return queryResult.query_result;
    }


    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);


    std::vector<Document> AddFindRequest(const std::string& raw_query);


    int GetNoResultRequests() const;

private:
    struct QueryResult {
        bool isEmpty;
        std::vector<Document> query_result;
    };

    std::deque<QueryResult> requests_;
    int noResultRequests_ = 0;
    constexpr static int sec_in_day_ = 1440;
    int seconds = 0;
    const SearchServer& search_server_;
};