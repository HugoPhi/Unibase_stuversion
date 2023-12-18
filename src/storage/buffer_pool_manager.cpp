#include "./buffer_pool_manager.h"

bool BufferPoolManager::find_victim_page(frame_id_t *frame_id) {
    if (!free_list_.empty()) {
        *frame_id = free_list_.front();
        free_list_.pop_front();
        return true;
    }
    return replacer_->victim(frame_id);
}

void BufferPoolManager::update_page(Page *page, PageId new_page_id, frame_id_t new_frame_id) {
    if (page->is_dirty()) {
        PageId temp = page->get_page_id();
        int fd_ = temp.fd;
        disk_manager_->write_page(fd_, temp.page_no, page->get_data(), PAGE_SIZE);
        page->is_dirty_ = false;
        page->pin_count_ = 0;
    }

    page_table_.erase(page->id_);
    page_table_[new_page_id] = new_frame_id;

    page->reset_memory();
    Page *newpage = pages_ + new_frame_id;
    newpage->id_ = new_page_id;
    newpage->pin_count_ = 1;
    disk_manager_->read_page(new_page_id.fd, new_page_id.page_no, page->get_data(), PAGE_SIZE);
    replacer_->pin(new_frame_id);
}

Page *BufferPoolManager::fetch_page(PageId page_id) {
    std::scoped_lock lock{latch_};

    if (page_table_.find(page_id) != page_table_.end()) {
        Page *page = pages_ + page_table_[page_id];
        if (page->pin_count_ == 0)
            replacer_->pin(page_table_[page_id]);
        page->pin_count_++;
        return page;
    }

    frame_id_t frameid;
    if (!find_victim_page(&frameid))
        return nullptr;
    Page *page = pages_ + frameid;
    update_page(page, page_id, frameid);
    return page;
}

bool BufferPoolManager::unpin_page(PageId page_id, bool is_dirty) {
    std::scoped_lock lock{latch_};
    if (page_table_.find(page_id) == page_table_.end())
        return false;

    Page *page = pages_ + page_table_[page_id];
    if (page->pin_count_ <= 0)
        return false;
    page->pin_count_--;
    if (!page->pin_count_)
        replacer_->unpin(page_table_[page_id]);

    page->is_dirty_ = is_dirty;
    return true;
}

bool BufferPoolManager::flush_page(PageId page_id) {
    std::scoped_lock lock{latch_};
    if (page_table_.find(page_id) == page_table_.end())
        return false;

    Page *page = pages_ + page_table_[page_id];
    disk_manager_->write_page(page->id_.fd, page->id_.page_no, page->get_data(), PAGE_SIZE);
    page->is_dirty_ = false;
    return true;
}

Page *BufferPoolManager::new_page(PageId *page_id) {
    std::scoped_lock lock{latch_};
    frame_id_t frameid;
    if (!find_victim_page(&frameid))
        return nullptr;
    Page *page = pages_ + frameid;

    if (page->is_dirty()) {
        disk_manager_->write_page(page->id_.fd, page->id_.page_no, page->get_data(), PAGE_SIZE);
        page->pin_count_ = 0;
        page->is_dirty_ = false;
    }
    page_id->page_no = disk_manager_->allocate_page(page_id->fd);

    page->reset_memory();
    page_table_.erase(page->get_page_id());
    page_table_[*page_id] = frameid;

    page->pin_count_ = 1;
    page->id_ = *page_id;
    replacer_->pin(frameid);

    return page;
}

bool BufferPoolManager::delete_page(PageId page_id) {
    std::scoped_lock lock{latch_};
    if (page_table_.find(page_id) == page_table_.end())
        return true;
    Page *page = pages_ + page_table_[page_id];
    if (page->pin_count_ > 0)
        return false;
    disk_manager_->deallocate_page(page_id.page_no);

    free_list_.push_back(page_table_[page_id]);
    page_table_.erase(page_id);

    page->is_dirty_ = false;
    page->pin_count_ = 0;
    page->id_.page_no = INVALID_PAGE_ID;

    return true;
}

void BufferPoolManager::flush_all_pages(int fd) {
    std::scoped_lock lock{latch_};
    for (size_t i = 0; i < pool_size_; i++) {
        Page *page = &pages_[i];
        if (page->get_page_id().fd == fd && page->get_page_id().page_no != INVALID_PAGE_ID) {
            disk_manager_->write_page(page->get_page_id().fd, page->get_page_id().page_no, page->get_data(), PAGE_SIZE);
            page->is_dirty_ = false;
        }
    }
}



