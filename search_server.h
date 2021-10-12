#pragma once

#include <vector>
#include <string>
#include <set>
#include <map>
#include <algorithm>
#include <stdexcept>
#include <execution>
#include <type_traits>
#include <mutex>

#include "document.h"
#include "log_duration.h"
#include "string_processing.h"
#include "concurrent_map.h"


class SearchServer {
public:

    inline static constexpr int CONCURRENT_MEP_THREADS_COUNT = 500;
    inline static constexpr int MAX_RESULT_DOCUMENT_COUNT = 5;
    inline static constexpr double eps = 1e-6;
    inline static constexpr int INVALID_DOCUMENT_ID = -1;


    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
    {
        using namespace std::string_literals;
        if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
            throw std::invalid_argument("Some of stop words are invalid"s);
        }
    }


    explicit SearchServer(const std::string& stop_words_text);


    explicit SearchServer(const std::string_view stop_words_text);


    explicit SearchServer();


    void AddDocument(int document_id, const std::string_view& document, DocumentStatus status, const std::vector<int>& ratings);


    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const {
        using namespace std::literals;
        CheckQuery(raw_query.data());
        LOG_DURATION("FindTopDocuments operation time"s);

        const auto query = ParseQuery(raw_query.data());
        auto matched_documents = FindAllDocuments(query, document_predicate);
        sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < eps) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }


    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const;


    std::vector<Document> FindTopDocuments(const std::string_view& raw_query) const;


    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(ExecutionPolicy& policy, const std::string_view& raw_query, DocumentPredicate document_predicate) const {
        if (!IsExecutionPolicyParallel(policy)) {
            return FindTopDocuments(raw_query, document_predicate);
        }

        using namespace std::literals;
        CheckQuery(raw_query.data());
        LOG_DURATION("Parallel FindTopDocuments operation time"s);

        const auto query = ParseQuery(raw_query.data());
        auto matched_documents = FindAllDocuments(policy, query, document_predicate);
        sort(std::execution::par, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < eps) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;

    }


    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy& policy, const std::string_view& raw_query, DocumentStatus status) const {
        if (IsExecutionPolicyParallel(policy)) {
            return FindTopDocuments(policy, raw_query, [status]([[maybe_unused]] int document_id, [[maybe_unused]] DocumentStatus document_status, [[maybe_unused]] int rating) {
                return document_status == status;
                });
        } else {
            return FindTopDocuments(raw_query, status);
        }
    }


    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy& policy, const std::string_view& raw_query) const {
        if (IsExecutionPolicyParallel(policy)) {
            return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
        } else {
            return FindTopDocuments(raw_query);
        }
    }


    int GetDocumentCount() const;


    void SetStopWords(const std::string& text);


    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view& raw_query, int document_id) const;


    template <typename ExecutionPolicy>
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(ExecutionPolicy& policy, const std::string_view& raw_query, int document_id) const {
        if (!IsExecutionPolicyParallel(policy)) {
            return MatchDocument(raw_query, document_id);
        }

        using namespace std::literals;
        CheckQuery(raw_query.data());
        if (!IsIDValid(document_ids_, document_id, true)) {
            throw std::out_of_range("document's id is out of range");
        }
        LOG_DURATION("Parallel MathDocument operation time"s);

        const auto query = ParseQuery(raw_query.data());
        static std::vector<std::string> matched_words;
        matched_words.clear();

        for (const std::string& word : query.plus_words) {
            if (!DoesMapContainSuchKey(word_to_document_freqs_, word)) {
                continue;
            }
            if (DoesMapContainSuchKey(word_to_document_freqs_.at(word), document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const std::string& word : query.minus_words) {
            if (!DoesMapContainSuchKey(word_to_document_freqs_, word)) {
                continue;
            }
            if (DoesMapContainSuchKey(word_to_document_freqs_.at(word), document_id)) {
                matched_words.clear();
                break;
            }
        }

        static std::vector<std::string_view> matched_words_to_return;
        matched_words_to_return.clear();

        for (size_t i = 0; i < matched_words.size(); ++i) {
            matched_words_to_return.push_back(matched_words[i]);
        }

        return { matched_words_to_return, documents_.at(document_id).status };
    }


    const std::map<std::string, double>& GetWordFrequencies(int document_id) const;


    void RemoveDocument(int document_id);


    template <typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy policy, int document_id) {
        if (!IsExecutionPolicyParallel(policy)) {
            RemoveDocument(document_id);
            return;
        }

        if (!IsIDValid(document_ids_, document_id, true)) {
            return;
        }

        for (auto& [_, documentIdToTF] : word_to_document_freqs_) {
            if (documentIdToTF.count(document_id)) {
                documentIdToTF.erase(document_id);
            }
        }
        document_id_to_words_freq_.erase(document_id);
        documents_.erase(document_id);
        auto iter = find(std::execution::par, document_ids_.begin(), document_ids_.end(), document_id);
        document_ids_.erase(iter);
    }


    std::vector<int>::const_iterator begin() const;


    std::vector<int>::const_iterator end() const;


    int GetDocumentId(int index) const;

private:
    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };


    struct DocumentData {
        int rating;
        DocumentStatus status;
    };


    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };


    std::set<std::string> stop_words_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string, double>> document_id_to_words_freq_ = { {-1, {} } };
    std::map<int, DocumentData> documents_;
    std::vector<int> document_ids_;


    template <typename ExecutionPolicy>
    static bool IsExecutionPolicyParallel(ExecutionPolicy&) {
        if (std::is_same_v<std::decay_t<ExecutionPolicy>, std::execution::parallel_policy>) {
            return true;
        }
        return false;
    }


    template <typename Type, typename AnotherType>
    static bool DoesMapContainSuchKey(std::map<Type, AnotherType> container, Type item) {
        std::vector<Type> vector_of_keys(container.size());
        for (auto [key, _] : container) {
            vector_of_keys.push_back(key);
        }
        return std::count(std::execution::par, vector_of_keys.begin(), vector_of_keys.end(), item);
    }


    static bool IsIDValid(std::vector<int> document_ids, int document_id, bool multithreading);


    static void CheckQuery(const std::string& query);


    static void AreMinusWordsCorrect(const std::string& word);


    static bool IsValidWord(const std::string& word);


    bool IsStopWord(const std::string& word) const;


    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;


    static int ComputeAverageRating(const std::vector<int>& ratings);


    QueryWord ParseQueryWord(std::string text) const;


    Query ParseQuery(const std::string& text) const;


    double ComputeWordInverseDocumentFreq(const std::string& word) const;


    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        std::map<int, double> document_to_relevance;

        for (const std::string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const std::string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        std::vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }


    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(ExecutionPolicy& policy, const Query& query, DocumentPredicate document_predicate) const {
        if (!IsExecutionPolicyParallel(policy)) {
            return FindAllDocuments(query, document_predicate);
        }

        ConcurrentMap<int, double> document_to_relevance(CONCURRENT_MEP_THREADS_COUNT);

        std::for_each(std::execution::par,
            query.plus_words.begin(), query.plus_words.end(),
            [this, &document_to_relevance, &document_predicate](const auto& word) {

                if (word_to_document_freqs_.count(word) == 0) {
                    return;
                }
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                    const auto& document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating)) {
                        document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                    }
                }

            });

        std::for_each(std::execution::par,
            query.minus_words.begin(), query.minus_words.end(),
            [this, &document_to_relevance](const auto& word) {

                if (word_to_document_freqs_.count(word) == 0) {
                    return;
                }
                for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                    document_to_relevance.Erase(document_id);
                }

            }
        );

        std::vector<Document> matched_documents;
        auto document_to_relevance_ordinary_map = document_to_relevance.BuildOrdinaryMap();
        std::mutex vector_mutex;
        std::for_each(
            std::execution::par,
            document_to_relevance_ordinary_map.begin(), document_to_relevance_ordinary_map.end(),
            [this, &matched_documents, &vector_mutex](auto& item) {
                Document item_to_move = { item.first, item.second, documents_.at(item.first).rating };
                std::lock_guard<std::mutex> guard(vector_mutex);
                matched_documents.push_back(std::move(item_to_move));
            }
        );

        return matched_documents;
    }
};