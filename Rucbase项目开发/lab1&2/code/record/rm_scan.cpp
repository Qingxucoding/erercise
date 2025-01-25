/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "rm_scan.h"

#include "rm_file_handle.h"

/**
 * @brief ��ʼ��file_handle��rid
 * @param file_handle
 */

RmScan::RmScan(const RmFileHandle* file_handle) : file_handle_(file_handle) {
    // Todo:
    // ��ʼ��file_handle��rid��ָ���һ������˼�¼��λ�ã�
    // ��ʼ��ridΪ��һ����¼ҳ���δʹ�õĲ��
    rid_ = { RM_FIRST_RECORD_PAGE, -1 };
    if (rid_.page_no < file_handle_->file_hdr_.num_pages) {
        next();  // ��ʼѰ�ҵ�һ����Ч�ļ�¼
        return;
    }
    rid_.page_no = RM_NO_PAGE;  // ����Ϊ��Ч״̬
}

/**
 * @brief �ҵ��ļ�����һ������˼�¼��λ��
 */

void RmScan::next() {
    // Todo:
    // Ŀ�꣺Ѱ���ļ�����һ�����м�¼�Ŀ��ò�ۣ�ʹ�� rid_ ָ���λ��
    const int max_records = file_handle_->file_hdr_.num_records_per_page;  // ÿҳ����¼��
    const int page_max = file_handle_->file_hdr_.num_pages;                // ��ҳ��

    // ���浱ǰҳ��ľ��
    RmPageHandle page_handle = file_handle_->fetch_page_handle(rid_.page_no);           // ��ȡ��ǰҳ����
    rid_.slot_no = Bitmap::next_bit(1, page_handle.bitmap, max_records, rid_.slot_no);  // ������һ����Ч���

    // ����������Ч��ۣ�ֱ���ҵ��򳬳�ҳ�淶Χ
    while (rid_.slot_no == max_records) {
        file_handle_->buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(), false);  // ������ǰҳ��
        rid_.page_no++;  // ת����һ��ҳ��
        if (rid_.page_no >= page_max) {
            return;  // ������ҳ����������
        }
        // �����ﻺ�浱ǰҳ��ľ��
        page_handle = file_handle_->fetch_page_handle(rid_.page_no);           // ��ȡ��һ��ҳ��ľ��
        rid_.slot_no = Bitmap::first_bit(1, page_handle.bitmap, max_records);  // ���Ҹ�ҳ��ĵ�һ����Ч���
    }
}
/**
 * @brief �ж��Ƿ񵽴��ļ�ĩβ
 */
bool RmScan::is_end() const {
    // Todo: �޸ķ���ֵ
    // �����ǰҳ�Ѿ��ﵽ����¼��������ҳ�������ļ�����ҳ��������Ϊ�����β
    bool is_slot_full = (rid_.slot_no == file_handle_->file_hdr_.num_records_per_page);
    bool is_page_out_of_bounds = (rid_.page_no >= file_handle_->file_hdr_.num_pages);
    return is_slot_full && is_page_out_of_bounds;
}
/**
 * @brief RmScan�ڲ���ŵ�rid
 */
Rid RmScan::rid() const { return rid_; }
