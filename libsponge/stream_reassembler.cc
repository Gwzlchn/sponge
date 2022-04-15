#include "stream_reassembler.hh"

#include "iostream"
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

// template <typename... Targs>
// void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _str_to_assemble(), _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t cur_win_max_idx = _first_unassembled_index + _capacity - _output.buffer_size();
    if (index >= cur_win_max_idx)
        return;

    if (eof) {
        _eof = true;
    }
    // cut the chars which are out of window or have been assembled
    size_t data_start_idx = max(index, _first_unassembled_index);
    size_t data_end_idx = min(cur_win_max_idx, index + data.size());
    if (data_end_idx >= data_start_idx) {
        pair<size_t, size_t> cur_data_start_end_index = make_pair(data_start_idx, data_end_idx);
        string cur_data = data.substr(data_start_idx - index, data_end_idx - data_start_idx + 1);
        // insert current data to str_to_assemble map
        recv_bytes_t cur_recv_bytes = {cur_data_start_end_index, cur_data};
        while (!this->empty()) {
            auto iter = _str_to_assemble.lower_bound(cur_data_start_end_index);
            bool cur_data_could_merge_right =
                (iter != _str_to_assemble.end()) && (cur_recv_bytes.first.second >= iter->first.first);
            if (cur_data_could_merge_right) {
                cur_recv_bytes = merge_two_unassembled_strs(cur_recv_bytes, *iter);
                _unassembled_bytes -= iter->second.size();
                _str_to_assemble.erase(iter);
                iter = _str_to_assemble.lower_bound(cur_data_start_end_index);
            }

            bool cur_data_could_merge_left =
                (iter != _str_to_assemble.begin()) && ((--iter)->first.second >= cur_recv_bytes.first.first);
            if (cur_data_could_merge_left) {
                cur_recv_bytes = merge_two_unassembled_strs(*iter, cur_recv_bytes);
                _unassembled_bytes -= iter->second.size();
                _str_to_assemble.erase(iter);
            }
            if (!cur_data_could_merge_right && !cur_data_could_merge_left) {
                break;
            }
        }
        // insert the unassembled str to map
        _str_to_assemble.insert(cur_recv_bytes);
        _unassembled_bytes += cur_recv_bytes.second.size();
    }

    // if the first chunk start index is smaller than _first_unassembled_index
    // write the first chunk to output bytestream
    auto iter = _str_to_assemble.begin();
    if (!_str_to_assemble.empty() && (iter->first.first <= _first_unassembled_index)) {
        auto temp_map_head = *iter;
        _str_to_assemble.erase(iter);
        size_t written_len = _output.write(temp_map_head.second);
        _unassembled_bytes -= written_len;
        if (written_len == temp_map_head.second.size()) {
            // The first chunk was all written to the output stream
            _first_unassembled_index = temp_map_head.first.first + temp_map_head.second.size();
        } else {
            // Part of first chunk was written to the output stream
            size_t new_data_start_index = temp_map_head.first.first + written_len;
            _str_to_assemble.insert(
                {{new_data_start_index, temp_map_head.first.second}, temp_map_head.second.substr(written_len)});
            _first_unassembled_index = new_data_start_index;
        }
    }
    if (empty() && _eof) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _unassembled_bytes == 0; }

//! \details This function merge two recv_bytes_type pairs, the start index of a is always smaller than b.
StreamReassembler::recv_bytes_t StreamReassembler::merge_two_unassembled_strs(
    const StreamReassembler::recv_bytes_t &a,
    const StreamReassembler::recv_bytes_t &b) const {
    recv_bytes_t res;
    res.first.first = a.first.first;
    // choose the bigger one for the end index of merged string.
    if (a.first.second > b.first.second) {
        res.first.second = a.first.second;
        res.second = a.second;
    } else {
        res.first.second = b.first.second;
        res.second = a.second.substr(0, b.first.first - a.first.first) + b.second;
    }
    return res;
}

void StreamReassembler::debug_strs_to_assemble(const map<pair<size_t, size_t>, string> &_str_to_assemble) {
    cerr << "Start Index \t End Index \n";
    for (const auto &iter : _str_to_assemble) {
        cerr << "\t" << iter.first.first << "\t\t" << iter.first.second << "\t" << iter.second << endl;
    }
}

void StreamReassembler::debug_recv_bytes(const StreamReassembler::recv_bytes_t &a) {
    printf("Start Index: %ld \t End Index: %ld \t String %s \n", a.first.first, a.first.second, a.second.c_str());
}
