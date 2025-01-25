/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "buffer_pool_manager.h"

/**
 * @description: ��free_list��replacer�еõ�����̭֡ҳ�� *frame_id
 * @return {bool} true: ���滻֡���ҳɹ� , false: ���滻֡����ʧ��
 * @param {frame_id_t*} frame_id ֡ҳidָ��,���سɹ��ҵ��Ŀ��滻֡id
 */
bool BufferPoolManager::find_victim_page(frame_id_t* frame_id) {
    // Todo:
    // 1 ʹ��BufferPoolManager::free_list_�жϻ�����Ƿ�������Ҫ��̭ҳ��
    // 1.1 δ�����frame
    if (!free_list_.empty()) {
        *frame_id = free_list_.front();
        free_list_.pop_front();
        return true;
    }
    // 1.2 ����ʹ��lru_replacer�еķ���ѡ����̭ҳ��
    else
        return replacer_->victim(frame_id);
    // return false;
}

/**
 * @description: ����ҳ������, ���Ϊ��ҳ����д����̣��ٸ���Ϊ��ҳ�棬����pageԪ����(data, is_dirty, page_id)��page
 * table
 * @param {Page*} page д��ҳָ��
 * @param {PageId} new_page_id �µ�page_id
 * @param {frame_id_t} new_frame_id �µ�֡frame_id
 */
void BufferPoolManager::update_page(Page* page, PageId new_page_id, frame_id_t new_frame_id) {
    // Todo:
    // 1 �������ҳ��д�ش��̣����Ұ�dirty��Ϊfalse
    if (page->is_dirty_) {  // ��λ����
        this->disk_manager_->write_page(page->get_page_id().fd, page->get_page_id().page_no, page->get_data(),
            PAGE_SIZE);
        page->is_dirty_ = false;
    }

    // 2 ����page table
    // for(auto position = this->page_table_.begin(); position != this->page_table_.end(); position++) {  //����table
    //     if(position->first == page->id_) {
    //         this->page_table_.erase(position);
    //         break;
    //     }
    // }
    auto it = page_table_.find(page->id_);
    if (it != page_table_.end()) {
        this->page_table_.erase(it);
    }
    page_table_[new_page_id] = new_frame_id;

    // 3 ����page��data������page id
    page->reset_memory();
    page->id_ = new_page_id;
}

/**
 * @description: ��buffer pool��ȡ��Ҫ��ҳ��
 *              ���ҳ���д���page_id��˵����page�ڻ�����У�������pin_count++��
 *              ���ҳ������page_id��˵����page�ڴ����У������һ����victim
 * page�������滻Ϊ�����ж�ȡ��page��pin_count��1��
 * @return {Page*} ���������Ҫ��ҳ���䷵�أ����򷵻�nullptr
 * @param {PageId} page_id ��Ҫ��ȡ��ҳ��PageId
 */
Page* BufferPoolManager::fetch_page(PageId page_id) {
    // Todo:
    std::scoped_lock lock{ latch_ };
    // 1.     ��page_table_����ѰĿ��ҳ
    auto it = page_table_.find(page_id);
    // 1.1    ��Ŀ��ҳ�б�page_table_��¼����������frame�̶�(pin)��������Ŀ��ҳ��
    if (it != page_table_.end()) {
        frame_id_t frame_id = it->second;  // ��ȡҳ�����ڵ�֡��
        Page* page = &pages_[frame_id];    // �� pages_ �����ж�λҳ��
        this->replacer_->pin(frame_id);
        page->pin_count_++;
        return page;  // ����ҳ������
    }

    // 1.2    ���򣬳��Ե���find_victim_page���һ�����õ�frame����ʧ���򷵻�nullptr
    frame_id_t avaliable_frame_id;
    if (!find_victim_page(&avaliable_frame_id)) {
        return nullptr;
    }
    // 2.     UpdatePage����������õĿ���frame�洢��Ϊdirty page���������updata_page��pageд�ص�����
    update_page(&pages_[avaliable_frame_id], page_id, avaliable_frame_id);
    // 3.     ����disk_manager_��read_page��ȡĿ��ҳ��frame
    Page* page = &(pages_[avaliable_frame_id]);  // ͨ��id��ȡpage
    disk_manager_->read_page(page_id.fd, page_id.page_no, page->get_data(), PAGE_SIZE);
    // 4.     �̶�Ŀ��ҳ������pin_count_
    replacer_->pin(avaliable_frame_id);
    pages_[avaliable_frame_id].pin_count_ = 1;
    // 5.     ����Ŀ��ҳ
    return &pages_[avaliable_frame_id];
}

/**
 * @description: ȡ���̶�pin_count>0���ڻ�����е�page
 * @return {bool} ���Ŀ��ҳ��pin_count<=0�򷵻�false�����򷵻�true
 * @param {PageId} page_id Ŀ��page��page_id
 * @param {bool} is_dirty ��Ŀ��pageӦ�ñ����Ϊdirty��Ϊtrue������Ϊfalse
 */
bool BufferPoolManager::unpin_page(PageId page_id, bool is_dirty) {
    // Todo:
    // 0. lock latch
    std::scoped_lock lock{ latch_ };
    // 1. ������page_table_����Ѱpage_id��Ӧ��ҳP
    auto it = page_table_.find(page_id);
    // 1.1 P��ҳ���в����� return false
    if (it == page_table_.end()) {
        return false;
    }
    // 1.2 P��ҳ���д��ڣ���ȡ��pin_count_
    Page& page = pages_[it->second];  ////////////////////ע��������
    // 2.1 ��pin_count_�Ѿ�����0���򷵻�false
    if (page.pin_count_ == 0) {
        return false;
    }
    // 2.2 ��pin_count_����0����pin_count_�Լ�һ
    if (page.pin_count_ > 0) {
        page.pin_count_--;
        // 2.2.1 ���Լ������0�������replacer_��Unpin
        if (page.pin_count_ == 0) {
            replacer_->unpin(it->second);
        }
    }

    // 3 ���ݲ���is_dirty������P��is_dirty_
    page.is_dirty_ = is_dirty;
    return true;
}

/**
 * @description: ��Ŀ��ҳд�ش��̣������ǵ�ǰҳ���Ƿ����ڱ�ʹ��
 * @return {bool} �ɹ��򷵻�true�����򷵻�false(ֻ��page_table_��û��Ŀ��ҳʱ)
 * @param {PageId} page_id Ŀ��ҳ��page_id������ΪINVALID_PAGE_ID
 */
bool BufferPoolManager::flush_page(PageId page_id) {
    // Todo:
    // 0. lock latch
    std::scoped_lock lock{ latch_ };
    // 1. ����ҳ��,���Ի�ȡĿ��ҳP
    auto it = page_table_.find(page_id);
    // 1.1 P��ҳ���в����� return false
    // 1.1 Ŀ��ҳPû�б�page_table_��¼ ������false
    if (it == page_table_.end()) {
        return false;
    }

    // 2. ����P�Ƿ�Ϊ�඼����д�ش��̡�
    Page& page = pages_[it->second];
    disk_manager_->write_page(page_id.fd, page_id.page_no, page.get_data(), PAGE_SIZE);
    // 3. ����P��is_dirty_
    page.is_dirty_ = false;
    return true;
}

/**
 * @description: ����һ���µ�page�����Ӵ������ƶ�һ���½��Ŀ�page�������ĳ��λ�á�
 * @return {Page*} �����´�����page��������ʧ���򷵻�nullptr
 * @param {PageId*} page_id ���ɹ�����һ���µ�pageʱ�洢��page_id
 */
Page* BufferPoolManager::new_page(PageId* page_id) {
    std::scoped_lock lock{ latch_ };
    // 1.   ���һ�����õ�frame�����޷�����򷵻�nullptr
    frame_id_t avaliable_frame_id;
    if (!find_victim_page(&avaliable_frame_id)) {
        return nullptr;
    }

    // 2.   ��fd��Ӧ���ļ�����һ���µ�page_id
    page_id->page_no = disk_manager_->allocate_page(page_id->fd);
    // 3.   ��frame������д�ش���
    update_page(&pages_[avaliable_frame_id], *page_id, avaliable_frame_id);
    // 4.   �̶�frame������pin_count_
    replacer_->pin(avaliable_frame_id);
    pages_[avaliable_frame_id].pin_count_ = 1;
    // 5.     ���ػ�õ�page
    return &pages_[avaliable_frame_id];
}

/**
 * @description: ��buffer_poolɾ��Ŀ��ҳ
 * @return {bool} ���Ŀ��ҳ��������buffer_pool���߳ɹ���ɾ���򷵻�true�����������buffer_pool���޷�ɾ���򷵻�false
 * @param {PageId} page_id Ŀ��ҳ
 */
bool BufferPoolManager::delete_page(PageId page_id) {
    std::scoped_lock lock{ latch_ };
    // 1.   ��page_table_�в���Ŀ��ҳ���������ڷ���true
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) {
        return true;
    }
    // 2.   ��Ŀ��ҳ��pin_count��Ϊ0���򷵻�false
    Page& target_page = pages_[it->second];
    if (target_page.pin_count_ != 0) {
        return false;
    }
    // 3.   ��Ŀ��ҳ����д�ش��̣���ҳ����ɾ��Ŀ��ҳ��������Ԫ����(data, is_dirty,
    // page_id)���������free_list_������true 3.1  ��Ŀ��ҳ����д�ش���
    disk_manager_->write_page(page_id.fd, page_id.page_no, target_page.get_data(), PAGE_SIZE);
    // 3.2 ��ҳ����ɾ��Ŀ��ҳ
    page_table_.erase(page_id);
    // 3.3 ������Ԫ����(data, is_dirty, page_id)
    target_page.is_dirty_ = false;
    target_page.reset_memory();
    // 3.4 �������free_list_
    free_list_.push_back(it->second);
    return true;
}

/**
 * @description: ��buffer_pool�е�����ҳд�ص�����
 * @param {int} fd �ļ����
 */
void BufferPoolManager::flush_all_pages(int fd) {
    std::scoped_lock lock{ latch_ };
    for (size_t i = 0; i < pool_size_; i++) {
        Page& page = pages_[i];
        if (page.get_page_id().fd == fd && page.get_page_id().page_no != INVALID_PAGE_ID) {
            disk_manager_->write_page(page.get_page_id().fd, page.get_page_id().page_no, page.get_data(), PAGE_SIZE);
            page.is_dirty_ = false;
        }
    }
}
