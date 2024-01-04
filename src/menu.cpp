#include "menu.h"

//
// MenuItem implementation
//

MenuItem::MenuItem(const char *title, const char *audioFilename, Menu *submenu, void (*callback)(MenuItem *)) {
    if (title) {
        this->title = strdup(title);
    } else {
        this->title = NULL;
    }
    if (audioFilename) {
        this->audioFilename = strdup(audioFilename);
    } else {
        this->audioFilename = NULL;
    }
    this->submenu = submenu;
    this->callback = callback;
}

MenuItem::~MenuItem() {
    if (submenu) {
        delete submenu;
    }
    if (title) {
        free(title);
    }
    if (audioFilename) {
        free(audioFilename);
    }
}

Menu* MenuItem::call() {
    if (this->callback) {
        this->callback(this);
    }
    if (this->submenu) {
        return this->submenu;
    }
    return NULL;
}

const char* MenuItem::getDisplayTitle() {
    return this->title;
}

const char* MenuItem::getAudioFile() {
    return this->audioFilename;
}

Menu* MenuItem::getSubmenu() {
    return this->submenu;
}

//
// Menu Implementation
//

Menu::Menu() {
    this->numItems = 0;
    this->state = StateInMenu;
}

Menu::~Menu() {
    for (int i = 0; i < numItems; i++) {
        delete items[i];
    }
    free(items);
}

void Menu::setItems(MenuItem **items) {
    // count items
    int numItems = 0;
    MenuItem *item = items[0];
    while (item != NULL) {
        numItems++;
        item = items[numItems];
    }

    Serial.printf_P(PSTR("Menu created, %d items\n"), numItems);

    // cleanup internal state
    if (this->numItems > 0) {
        for(int i = 0; i < this->numItems; i++) {
            delete this->items[i];
        }
        free(this->items);
    }
    
    // copy items over
    this->numItems = numItems;
    this->items = (MenuItem**)malloc(sizeof(MenuItem*) * numItems);
    for(int i = 0; i < numItems; i++) {
        this->items[i] = items[i];

        Menu *submenu = items[i]->getSubmenu();
        if (submenu) {
            submenu->state = StateUnknown;
        }
    }

    // reset state
    this->selectedItem = 0;
}
    
void Menu::setDisplayUpdateCallback(void (*callback)(const char *text)) {
    this->displayCallback = callback;
}

void Menu::setAudioAnnounceCallback(void (*callback)(const char* audioFilename)) {
    this->audioCallback = callback;
}

void Menu::setEnterCallback(void (*callback)(Menu *menu)) {
    this->enterCallback = callback;
}

void Menu::setLeaveCallback(void (*callback)(Menu *menu)) {
    this->leaveCallback = callback;
}


MenuItem* Menu::getItem(int index) {
    if ((index < 0) || (index >= this->numItems)) {
        return NULL;
    }
    return this->items[index];
}

MenuItem* Menu::selectNextItem() {
    MenuItem *item = NULL;

    Serial.printf_P(PSTR("Select next item: current = %d, state = %d\n"), this->selectedItem, this->state);

    // If in this menu, select next item, wrap around at end
    if (this->state == StateInMenu) {
        this->selectedItem++;
        if (this->selectedItem >= this->numItems) this->selectedItem = 0;
        item = this->items[this->selectedItem];
    }

    // If in a submenu, forward the call to the submenu
    if (this->state == StateInSubmenu) {
        Menu *submenu = this->items[this->selectedItem]->getSubmenu();
        item = submenu->selectNextItem();
    }

    if (item) {
        // run callbacks to update UI
        if (this->displayCallback) {
            this->displayCallback(item->getDisplayTitle());
        }
        if (this->audioCallback) {
            this->audioCallback(item->getAudioFile());
        }
    }

    return item;
};

MenuItem* Menu::selectPreviousItem() {
    MenuItem *item = NULL;

    Serial.printf_P(PSTR("Select prev item: current = %d, state = %d\n"), this->selectedItem, this->state);

    // If in this menu, select previous item, wrap around at beginning
    if (this->state == StateInMenu) {
        this->selectedItem--;
        if (this->selectedItem < 0) this->selectedItem = this->numItems - 1;
        item = this->items[this->selectedItem];
    }

    // If in a submenu, forward the call to the submenu
    if (this->state == StateInSubmenu) {
        Menu *submenu = this->items[this->selectedItem]->getSubmenu();
        item = submenu->selectPreviousItem();
    }

    if (item) {
        // run callbacks to update UI
        if (this->displayCallback) {
            this->displayCallback(item->getDisplayTitle());
        }
        if (this->audioCallback) {
            this->audioCallback(item->getAudioFile());
        }
    }

    return item;
}

Menu* Menu::enterItem() {
    Menu *submenu = NULL;

    Serial.printf_P(PSTR("Enter item: current = %d, state = %d\n"), this->selectedItem, this->state);

    if (this->state == StateInMenu) {
        submenu = this->items[this->selectedItem]->call();
        if (submenu) {
            this->state = StateInSubmenu;
            if (submenu->enterCallback) {
                submenu->enterCallback(submenu);
            }
        }
    }
    if (this->state == StateInSubmenu) {
        Menu *mySubmenu = this->items[this->selectedItem]->getSubmenu();
        submenu = mySubmenu->enterItem();
    }

    // run callbacks to update UI
    if (submenu) {
        MenuItem *firstItem = submenu->getItem(0);
        // run callbacks to update UI
        if (this->displayCallback) {
            this->displayCallback(firstItem->getDisplayTitle());
        }
        if (this->audioCallback) {
            this->audioCallback(firstItem->getAudioFile());
        }
    }

    return submenu;
}

Menu* Menu::leaveItem() {
    Serial.printf_P(PSTR("Leave item: current = %d, state = %d\n"), this->selectedItem, this->state);

    if (this->state == StateInMenu) {
        this->state = StateUnknown;
        this->selectedItem = 0;
    }

    if (this->state == StateInSubmenu) {
        MenuItem *item = NULL;
        Menu *submenu = this->items[this->selectedItem]->getSubmenu();
        Menu *newMenu = submenu->leaveItem();
        if (!newMenu) {
            this->state = StateInMenu;
            item = this->items[this->selectedItem];
            if (submenu->leaveCallback) {
                submenu->leaveCallback(submenu);
            }
        } else {
            item = newMenu->getItem(0);
        }

        // run callbacks to update UI
        if (item) {
            if (this->displayCallback) {
                this->displayCallback(item->getDisplayTitle());
            }
            if (this->audioCallback) {
                this->audioCallback(item->getAudioFile());
            }
        }
        return newMenu ? newMenu : this;
    }
    return NULL;
}

//
// Buttonmenu Implementation
//

ButtonMenu::ButtonMenu(void (*prev)(), void (*next)(), void (*enter)()) {
    this->prevCallback = prev;
    this->nextCallback = next;
    this->enterCallback = enter;

    this->numItems = 1;
    this->selectedItem = 0;
    this->state = StateInMenu;
}

ButtonMenu::~ButtonMenu() {
}

MenuItem* ButtonMenu::selectNextItem() {
    if (this->nextCallback) {
        this->nextCallback();
    }

    return NULL;
}

MenuItem* ButtonMenu::selectPreviousItem() {
    if (this->prevCallback) {
        this->prevCallback();
    }

    return NULL;
}

Menu * ButtonMenu::enterItem() {
    if (this->enterCallback) {
        this->enterCallback();
    }

    return NULL;
}
