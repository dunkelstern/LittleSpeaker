#ifndef LITTLESPEAKER_MENU_H
#define LITTLESPEAKER_MENU_H

#include <Arduino.h>
#include <SD.h>

typedef enum _MenuState {
    StateInMenu = 0,
    StateInSubmenu = 1
} MenuState;

class MenuItem;

//
// A statically defined menu, can contain sub-menus defined in Menu items
// on traversal it will call into the sub-menus automatically if the state
// declares it has entered a submenu
//
class Menu {
  public:
    Menu(MenuItem **items = NULL, void *context = NULL);
    Menu(void *context = NULL): Menu(NULL, context) {};
    ~Menu();

    void setItems(MenuItem **item); // close off menu with NULL value
    virtual MenuItem *getItem(int index);
    virtual int indexOfItem(MenuItem *item);

    virtual MenuItem *selectNextItem();
    virtual MenuItem *selectPreviousItem();
    virtual Menu *enterItem();
    virtual Menu *leaveItem();

    void setDisplayUpdateCallback(void (*callback)(const char *text));
    void setAudioAnnounceCallback(void (*callback)(const char *audioFilename));

    void setEnterCallback(void (*callback)(Menu *menu));
    void setLeaveCallback(void (*callback)(Menu *menu));

    void *getContext();

  protected:
    int numItems;
    int selectedItem;
    MenuState state;

  private:
    void *context;
    MenuItem **items;
    void (*displayCallback)(const char *text);
    void (*audioCallback)(const char *audioFilename);
    void (*enterCallback)(Menu *menu);
    void (*leaveCallback)(Menu *menu);

};

//
// A button menu which reacts to button presses directly without any menu items.
//
class ButtonMenu: public Menu {
    public:
        ButtonMenu(void (*prev)(Menu *menu), void (*next)(Menu *menu), void (*enter)(Menu *menu), void *context = NULL);
        ButtonMenu(void (*prev)(Menu *menu), void (*next)(Menu *menu), void (*enter)(Menu *menu), bool (*leave)(Menu *menu) = NULL, void *context = NULL);
        ~ButtonMenu();

        MenuItem *selectNextItem() override;
        MenuItem *selectPreviousItem() override;
        Menu *enterItem() override;
        Menu *leaveItem() override;
        MenuItem *getItem(int index) override;
        int indexOfItem(MenuItem *item) override;

    private:
        void (*prevCallback)(Menu *menu);
        void (*nextCallback)(Menu *menu);
        void (*enterCallback)(Menu *menu);
        bool (*leaveCallback)(Menu *menu);
};

//
// A menu item, can have a title, an audio file name (both optional) and
// either an attached sub-menu or a callback function to call when this
// menu item has been selected
//
class MenuItem {
  public:
    MenuItem(const char *title, const char *audioFilename, Menu *submenu = NULL, void (*callback)(MenuItem *) = NULL, void *context = NULL);
    MenuItem(const char *title, const char *audioFilename, void (*callback)(MenuItem *)) : MenuItem(title, audioFilename, NULL, callback, NULL) {};
    MenuItem(const char *title, const char *audioFilename, void (*callback)(MenuItem *), void *context) : MenuItem(title, audioFilename, NULL, callback, context) {};
    ~MenuItem();

    Menu *call();
    const char *getDisplayTitle();
    const char *getAudioFile();
    Menu *getSubmenu();
    void *getContext();

  private:
    const char *title;
    char *audioFilename;
    Menu *submenu;
    void *context;
    void (*callback)(MenuItem *item);
};

#endif