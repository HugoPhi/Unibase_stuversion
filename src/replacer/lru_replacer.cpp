#include "./lru_replacer.h"

LRUReplacer::LRUReplacer(size_t num_pages) { max_size_ = num_pages; }

LRUReplacer::~LRUReplacer() = default;  

/**
 * @description: 使用LRU策略删除一个victim frame，并返回该frame的id
 * @param {frame_id_t*} frame_id 被移除的frame的id，如果没有frame被移除返回nullptr
 * @return {bool} 如果成功淘汰了一个页面则返回true，否则返回false
 */
bool LRUReplacer::victim(frame_id_t* frame_id) {
    // C++17 std::scoped_lock
    // 它能够避免死锁发生，其构造函数能够自动进行上锁操作，析构函数会对互斥量进行解锁操作，保证线程安全。
    std::scoped_lock lock{latch_};  //  如果编译报错可以替换成其他lock

    // Todo:
    //  利用lru_replacer中的LRUlist_,LRUhash_实现LRU策略
    //  选择合适的frame指定为淘汰页面,赋值给*frame_id

    if (LRUlist_.empty()) { // 如果LRUlist为空, 返回false
        return false;
    }

    *frame_id = LRUlist_.back(); 
    LRUlist_.pop_back(); 
    LRUhash_.erase(*frame_id); 
    return true;
}

/**
 * @description: 固定指定的frame，即该页面无法被淘汰
 * @param {frame_id_t} 需要固定的frame的id
 */
void LRUReplacer::pin(frame_id_t frame_id) {
    std::scoped_lock lock{latch_};
    // Todo:
    // 固定指定id的frame
    // 在数据结构中移除该frame
    
    if (LRUhash_.find(frame_id) != LRUhash_.end()) {  // 如果frame已经在列表中
        LRUlist_.erase(LRUhash_[frame_id]);  // 从列表中移除
        LRUhash_.erase(frame_id);  // 从哈希表中移除
    }
}

/**
 * @description: 取消固定一个frame，代表该页面可以被淘汰
 * @param {frame_id_t} frame_id 取消固定的frame的id
 */
void LRUReplacer::unpin(frame_id_t frame_id) {
    // Todo:
    //  支持并发锁
    //  选择一个frame取消固定

    std::scoped_lock lock{latch_};
    if (LRUhash_.find(frame_id) == LRUhash_.end()) {  // 如果frame不在列表中
        if (LRUlist_.size() == max_size_) {  // 如果列表已满
            LRUhash_.erase(LRUlist_.back());  // 移除最不常用的frame
            LRUlist_.pop_back();
        }
        LRUlist_.push_front(frame_id);  // 将新的frame添加到列表前端
        LRUhash_[frame_id] = LRUlist_.begin();  // 在哈希表中添加新的frame
    }
}

/**
 * @description: 获取当前replacer中可以被淘汰的页面数量
 */
size_t LRUReplacer::Size() { 
    return LRUlist_.size(); 
}

