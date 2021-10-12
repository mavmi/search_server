#include "Remove_duplicates.h"


void RemoveDuplicates(SearchServer& search_server) {
    std::set<int> ids_to_delete;
    for (auto first_it = search_server.begin(); first_it != search_server.end(); ++first_it) {
        if (!ids_to_delete.count(*first_it)) {
            for (auto second_it = std::next(first_it); second_it != search_server.end(); ++second_it) {
                if (!ids_to_delete.count(*second_it)) {
                    std::set<std::string> first_document_content, second_document_content;
                    for (const auto& [word, _] : search_server.GetWordFrequencies(*first_it)) {
                        first_document_content.insert(word);
                    }
                    for (const auto& [word, _] : search_server.GetWordFrequencies(*second_it)) {
                        second_document_content.insert(word);
                    }
                    if (first_document_content == second_document_content) {
                        ids_to_delete.insert(*second_it);
                    }
                }
            }
        }
    }
    for (int id : ids_to_delete) {
        search_server.RemoveDocument(id);
    }
}