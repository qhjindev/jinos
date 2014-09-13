/*
 * list.h
 *
 *  Created on: Aug 15, 2013
 *      Author: qinghua
 */

#ifndef LIST_H_
#define LIST_H_

template<class T>
class list;

template<class T>
class list_iterator {
public:
  list_iterator(const list<T>& l) :
      current_(0), list_(&l) {
    this->current_ = static_cast<T*>(list_->head_->next_);
  }

  int first(void) {
    return this->go_head();
  }

  int advance(void) {
    return this->do_advance() ? 1 : 0;
  }

  //int next(T*&) const;

  T* next(void) const {
    return this->not_done();
  }

  int done(void) const {
    return this->not_done() ? 0 : 1;
  }

  T* not_done(void) const {
    if (this->current_ != this->list_->head_)
      return this->current_;
    else
      return 0;
  }

  //T& operator*(void) const {

  //}

  //void reset(List<T>&);
  list_iterator<T>& operator++(int) {
    list_iterator<T> retv(*this);

    this->do_advance();

    return retv;
  }

  list_iterator<T>& operator++(void) {
    this->do_advance();
    return *this;
  }

  list_iterator<T>& operator--(void) {
    this->do_retreat();
    return *this;
  }

  void dump(void) const {

  }

protected:
  //ListIterator(const List<T>& l) : current_(0), list_(&l) {

  //}

  list_iterator(const list_iterator<T> & iter) : current_(iter.current_), list_(iter.list_){

  }

  int go_head(void) {
    this->current_ = static_cast<T*>(list_->head_->next_);
    return this->current_ ? 1 : 0;
  }

  int go_tail(void) {
    this->current_ = static_cast<T*>(list_->head_->prev_);
    return this->current_ ? 1 : 0;
  }

  //T* done(void) const

  T* do_advance(void) {
    if (this->not_done()) {
      this->current_ = static_cast<T*>(this->current_->next_);
      return this->not_done();
    } else
      return 0;
  }

  T* do_retreat(void) {
    if (this->not_done()) {
      this->current_ = static_cast<T*> (this->current_->prev_);
      return this->not_done();
    } else
      return 0;
  }

  T* current_;

  const list<T>* list_;
};

template<class T>
class list {
public:
  friend class list_iterator<T>;

  list() {
    this->head_ = new T;
    init_head();
  }

  T* insert_tail(T* newItem) {
    this->insert_element(newItem, 1);
    return newItem;
  }

  T* insert_head(T* newItem) {
    insert_element(newItem);
    return newItem;
  }

  T* delete_head(void) {
    T* temp;

    if (is_empty())
      return 0;

    temp = static_cast<T*>(this->head_->next_);

    this->remove_element(temp);
    return temp;
  }

  T* delete_tail(void) {

    if (this->is_empty())
      return 0;

    T* temp = static_cast<T*> (this->head_->prev_);

    this->remove_element(temp);
    return temp;
  }

  size_t size(void) const {
    return this->size_;
  }

  void dump(void) const {

  }

  int remove(T* item) {
    return remove_element(item);
  }

  int is_empty(void) const {
    return this->size_ ? 0 : 1;
  }

protected:
  //void deleteNodes(void);

  //void copyNodes(const List<T>& rhs);

  void init_head(void) {
    this->head_->next_ = this->head_->prev_ = this->head_;
    this->size_ = 0;
  }

  int insert_element(T* newItem, int before = 0, T* oldItem = 0) {
    if (oldItem == 0)
      oldItem = this->head_;

    if (before)
      oldItem = oldItem->prev_;

    newItem->next_ = oldItem->next_;
    newItem->next_->prev_ = newItem;
    newItem->prev_ = oldItem;
    oldItem->next_ = newItem;
    this->size_++;
    return 0;
  }

  int remove_element(T* item) {
    if (item == this->head_ || item->next_ == 0 || item->prev_ == 0 || this->size() == 0)
      return -1;

    item->prev_->next_ = item->next_;
    item->next_->prev_ = item->prev_;
    item->next_ = item->prev_ = 0;
    this->size_--;
    return 0;
  }

  T* head_;

  size_t size_;
};




#endif /* LIST_H_ */
