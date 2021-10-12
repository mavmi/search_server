#include <stdexcept>
#include <math.h>
#include <iostream>

#include "search_server.h"


SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))
{
}


SearchServer::SearchServer(const std::string_view stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text.data()))
{
}


SearchServer::SearchServer() = default;


void SearchServer::AddDocument(int document_id, const std::string_view & document, DocumentStatus status, const std::vector<int>&ratings) {
    using namespace std::string_literals;
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("Invalid document_id"s);
    }
    const auto words = SplitIntoWordsNoStop(document.data());

    const double inv_word_count = 1.0 / words.size();
    for (const std::string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_id_to_words_freq_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.push_back(document_id);
}


std::vector<Document> SearchServer::FindTopDocuments(const std::string_view & raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status]([[maybe_unused]] int document_id, [[maybe_unused]] DocumentStatus document_status, [[maybe_unused]] int rating) {
        return document_status == status;
        });
}


std::vector<Document> SearchServer::FindTopDocuments(const std::string_view & raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}


int SearchServer::GetDocumentCount() const {
    return documents_.size();
}


void SearchServer::SetStopWords(const std::string & text) {
    for (const std::string& word : SplitIntoWords(text)) {
        stop_words_.insert(word);
    }
}


std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string_view & raw_query, int document_id) const {
    using namespace std::literals;
    
    std::string string_query = raw_query.data();
    CheckQuery(string_query);
    if (!IsIDValid(document_ids_, document_id, false)) {
        throw std::out_of_range("document's id is out of range");
    }
    LOG_DURATION("MathDocument operation time"s);

    const auto query = ParseQuery(string_query);
    static std::vector<std::string> matched_words;
    matched_words.clear();

    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word.data()).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
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


const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    return document_id_to_words_freq_.count(document_id) ? document_id_to_words_freq_.at(document_id) : document_id_to_words_freq_.at(-1);
}


void SearchServer::RemoveDocument(int document_id) {
    if (!IsIDValid(document_ids_, document_id, false)) {
        return;
    }

    for (auto& [_, documentIdToTF] : word_to_document_freqs_) {
        if (documentIdToTF.count(document_id)) {
            documentIdToTF.erase(document_id);
        }
    }
    document_id_to_words_freq_.erase(document_id);
    documents_.erase(document_id);
    auto iter = find(document_ids_.begin(), document_ids_.end(), document_id);
    document_ids_.erase(iter);
}


std::vector<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}


std::vector<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}


int SearchServer::GetDocumentId(int index) const {
    return document_ids_.at(index);
}


bool SearchServer::IsIDValid(std::vector<int> document_ids, int document_id, bool multithreading) {
    if (multithreading) {
        return std::count(std::execution::par, document_ids.begin(), document_ids.end(), document_id);
    } else {
        return std::count(document_ids.begin(), document_ids.end(), document_id);
    }
}


void SearchServer::CheckQuery(const std::string & query) {
    using namespace std::string_literals;
    for (const std::string& word : SplitIntoWords(query)) {
        AreMinusWordsCorrect(word);
        if (!IsValidWord(word)) {
            throw std::invalid_argument("word "s + word + " is not valid"s);
        }
    }
}


void SearchServer::AreMinusWordsCorrect(const std::string & word) {
    using namespace std::string_literals;
    //Check the number of minuses
    if (word.size() > 1 && (word[0] == '-' && word[1] == '-')) {
        throw std::invalid_argument("too much minuses in the minus-word "s + word);
    }
    //Check if the minus-word is empty
    else if (word.size() == 1 && word[0] == '-') {
        throw std::invalid_argument("minus-word is empty"s);
    }
}


bool SearchServer::IsValidWord(const std::string & word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}


bool SearchServer::IsStopWord(const std::string & word) const {
    return stop_words_.count(word) > 0;
}


std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string & text) const {
    using namespace std::string_literals;
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word "s + word + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}


int SearchServer::ComputeAverageRating(const std::vector<int>&ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}


SearchServer::QueryWord SearchServer::ParseQueryWord(std::string text) const {
    using namespace std::string_literals;
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty"s);
    }
    std::string word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("Query word "s + text + " is invalid");
    }

    return { word, is_minus, IsStopWord(word) };
}


SearchServer::Query SearchServer::ParseQuery(const std::string & text) const {
    Query result;
    for (const std::string& word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.insert(query_word.data);
            } else {
                result.plus_words.insert(query_word.data);
            }
        }
    }
    return result;
}


double SearchServer::ComputeWordInverseDocumentFreq(const std::string & word) const {
    return log(static_cast<double>(GetDocumentCount()) / word_to_document_freqs_.at(word).size());
}