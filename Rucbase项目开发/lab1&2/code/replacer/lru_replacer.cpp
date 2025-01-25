/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "lru_replacer.h"

LRUReplacer::LRUReplacer(size_t num_pages) { max_size_ = num_pages; }

LRUReplacer::~LRUReplacer() = default;

/**
 * @description: ʹ��LRU����ɾ��һ��victim frame�������ظ�frame��id
 * @param {frame_id_t*} frame_id ���Ƴ���frame��id�����û��frame���Ƴ�����nullptr
 * @return {bool} ����ɹ���̭��һ��ҳ���򷵻�true�����򷵻�false
 */
bool LRUReplacer::victim(frame_id_t* frame_id) {
    // C++17 std::scoped_lock
    // ���ܹ����������������乹�캯���ܹ��Զ�������������������������Ի��������н�����������֤�̰߳�ȫ��
    std::scoped_lock lock{ latch_ };  //  ������뱨������滻������lock

    // Todo:
    //  ����lru_replacer�е�LRUlist_,LRUHash_ʵ��LRU����
    //  ѡ����ʵ�frameָ��Ϊ��̭ҳ��,��ֵ��*frame_id
    if (LRUlist_.empty()) {
        return false;
    }
    *frame_id = LRUlist_.back();  // Get the least recently used frame_id
    LRUhash_.erase(*frame_id);    // Remove it from the hash table
    LRUlist_.pop_back();          // Remove it from the list
    return true;
}

/**
 * @description: �̶�ָ����frame������ҳ���޷�����̭
 * @param {frame_id_t} ��Ҫ�̶���frame��id
 */
void LRUReplacer::pin(frame_id_t frame_id) {
    std::scoped_lock lock{ latch_ };
    // Todo:
    // �̶�ָ��id��frame
    // �����ݽṹ���Ƴ���frame
    auto it = LRUhash_.find(frame_id);
    if (it != LRUhash_.end()) {
        LRUlist_.erase(it->second);  // Remove the frame_id from the list
        LRUhash_.erase(it);          // Remove the frame_id from the hash table
    }
}

/**
 * @description: ȡ���̶�һ��frame�������ҳ����Ա���̭
 * @param {frame_id_t} frame_id ȡ���̶���frame��id
 */
void LRUReplacer::unpin(frame_id_t frame_id) {
    // Todo:
    //  ֧�ֲ�����
    //  ѡ��һ��frameȡ���̶�
    // find != end
    std::lock_guard<std::mutex> guard(latch_);
    if (LRUhash_.find(frame_id) == LRUhash_.end() && Size() < max_size_) {
        LRUlist_.push_front(frame_id);          // Add the frame_id to the front of the list
        LRUhash_[frame_id] = LRUlist_.begin();  // Record its position in the hash table
    }
}

/**
 * @description: ��ȡ��ǰreplacer�п��Ա���̭��ҳ������
 */
size_t LRUReplacer::Size() { return LRUlist_.size(); }
