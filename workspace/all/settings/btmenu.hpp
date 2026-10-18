#pragma once

#include "menu.hpp"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace Bluetooth
{
    class PairingAgent;

    class Menu : public MenuList
    {
        const int &globalQuit;
        int &globalDirty;
        // bt on/off
        MenuItem *toggleItem;
        // diagnostics on/off
        MenuItem *diagItem;
        // max sample rate
        MenuItem *rateItem;
        
        std::thread worker;
        std::atomic<bool> quit{false};
        std::atomic<bool> page_active{false};
        std::atomic<bool> selectionDirty{false};
        std::atomic<bool> workerStarted{false};
        std::mutex wake_mu;
        std::condition_variable wake_cv;
        
        PairingAgent* pairingAgent = nullptr;
    public:
        Menu(const int &globalQuit, int &globalDirty);
        ~Menu();

        InputReactionHint handleInput(int &dirty, int &quit) override;
        void onShow() override;
        void onHide() override;

    private:
        std::any getBtToggleState() const;
        void setBtToggleState(const std::any &on);
        void resetBtToggleState();

        std::any getBtDiagnosticsState() const;
        void setBtDiagnosticsState(const std::any &on);
        void resetBtDiagnosticsState();

        std::any getSamplerateMaximum() const;
        void setSamplerateMaximum(const std::any &on);
        void resetSamplerateMaximum();

        void updater();
        void ensureWorker();
    };

    class PairableItem : public MenuItem
    {
        BT_device dev;

    public:
        PairableItem(BT_device d, MenuList *submenu);

        void drawCustomItem(SDL_Surface *surface, const SDL_Rect &dst, const AbstractMenuItem &item, bool selected) const override;
    };

    class PairedItem : public MenuItem
    {
        BT_devicePaired dev;

    public:
        PairedItem(BT_devicePaired d, MenuList *submenu);

        void drawCustomItem(SDL_Surface *surface, const SDL_Rect &dst, const AbstractMenuItem &item, bool selected) const override;
    };

    class ConnectKnownItem : public MenuItem
    {
        BT_devicePaired dev;
    public:
        ConnectKnownItem(BT_devicePaired n, std::atomic<bool>& dirty);
    };

    class DisconnectKnownItem : public MenuItem
    {
        BT_devicePaired dev;
    public:
        DisconnectKnownItem(BT_devicePaired n, std::atomic<bool>& dirty);
    };

    class PairNewItem : public MenuItem
    {
        BT_device dev;

    public:
        PairNewItem(BT_device n, std::atomic<bool>& dirty);
    };

    class UnpairItem : public MenuItem
    {
        BT_devicePaired dev;

    public:
        UnpairItem(BT_devicePaired n, std::atomic<bool>& dirty);
    };
}