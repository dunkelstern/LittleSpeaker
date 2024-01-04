#include <Arduino.h>
#include <SD.h>

typedef enum _MenuState {
    StateUnknown = 0,
    StateInMenu = 1,
    StateInSubmenu = 2
} MenuState;

class MenuItem;

//
// A statically defined menu, can contain sub-menus defined in Menu items
// on traversal it will call into the sub-menus automatically if the state
// declares it has entered a submenu
//
class Menu {
  public:
    Menu();
    ~Menu();

    void setItems(MenuItem **item); // close off menu with NULL value
    MenuItem *getItem(int index);

    virtual MenuItem *selectNextItem();
    virtual MenuItem *selectPreviousItem();
    virtual Menu *enterItem();
    virtual Menu *leaveItem();

    void setDisplayUpdateCallback(void (*callback)(const char *text));
    void setAudioAnnounceCallback(void (*callback)(const char *audioFilename));

    void setEnterCallback(void (*callback)(Menu *menu));
    void setLeaveCallback(void (*callback)(Menu *menu));

  protected:
    int numItems;
    int selectedItem;
    MenuState state;

  private:
    MenuItem **items;
    void (*displayCallback)(const char *text);
    void (*audioCallback)(const char *audioFilename);
    void (*enterCallback)(Menu *menu);
    void (*leaveCallback)(Menu *menu);

};

//
// A dynamically defined menu which generates it's items by calling into
// a callback function. One example would be a menu of files on an SD card
// with probably many files on it.
//
class DynamicMenu: public Menu {
    DynamicMenu(MenuItem *(*makeMenuItem)(int index, void *param), void *param);
    ~DynamicMenu();
};

//
// A button menu which reacts to button presses directly without any menu items.
//
class ButtonMenu: public Menu {
    public:
        ButtonMenu(void (*prev)(), void (*next)(), void (*enter)());
        ~ButtonMenu();

        MenuItem *selectNextItem() override;
        MenuItem *selectPreviousItem() override;
        Menu *enterItem() override;

    private:
        void (*prevCallback)();
        void (*nextCallback)();
        void (*enterCallback)();
};

//
// A menu item, can have a title, an audio file name (both optional) and
// either an attached sub-menu or a callback function to call when this
// menu item has been selected
//
class MenuItem {
  public:
    MenuItem(const char *title, const char *audioFilename, Menu *submenu = NULL, void (*callback)(MenuItem *) = NULL);
    ~MenuItem();

    Menu *call();
    const char *getDisplayTitle();
    const char *getAudioFile();
    Menu *getSubmenu();

  private:
    char *title;
    char *audioFilename;
    Menu *submenu;
    void (*callback)(MenuItem *item);
};