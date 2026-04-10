#ifndef HALMET_UI_COUNTER_H_
#define HALMET_UI_COUNTER_H_


namespace halmet {

    /**
     * A counter used to automatically supply unit sort order Ids to the ConfigItem->set_sort_order() method.
     */
    class UICounter {

        public:
           /**
            * Constructs a new UICounter object
            * 
            * @param increment The increment value to use for the next value
            * @param initialValue The initial value to use. If -1, the next value will be taken from the internal uiGroup_ object.
            */
           UICounter(int increment = 1, int initialValue = -1);

           int nextValue();

        private:
           static UICounter uiGroup_;
           int nextValue_;
           int increment_;
    };
   
}

#endif /* HALMET_UI_COUNTER_H_ */