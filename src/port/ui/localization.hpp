// Adapted from the Party Board (Marioparty4) port layer, credit: TwilitRealm
#pragma once

#include "port/settings.h"

#include <string>
#include <string_view>

namespace sms::ui {

inline std::string ui_translate(std::string_view text)
{
    if (sms::getSettings().game.language.getValue() != sms::GameLanguage::French) {
        return std::string(text);
    }
    struct Entry { std::string_view source; std::string_view translated; };
    static constexpr Entry entries[] = {
        {"Settings", "Paramètres"}, {"Quit", "Quitter"},
        {"Prelaunch", "Avant le lancement"}, {"Video", "Vidéo"}, {"Input", "Entrées"},
        {"Disc Image", "Image disque"}, {"Language", "Langue"}, {"Graphics Backend", "Moteur graphique"},
        {"Display", "Affichage"}, {"Resolution", "Résolution"}, {"Controller", "Manette"},
        {"Toggle Fullscreen", "Basculer en plein écran"}, {"Restore Default Window Size", "Restaurer la taille de fenêtre par défaut"},
        {"Enable VSync", "Activer la V-Sync"}, {"Frame Rate", "Fréquence d'images"},
        {"Lock 4:3 Aspect Ratio", "Verrouiller le format 4:3"}, {"Adaptive Widescreen HUD", "HUD écran large adaptatif"},
        {"Pause on Focus Lost", "Pause en perte de focus"}, {"Show FPS Counter", "Afficher le compteur d'images"},
        {"Internal Resolution", "Résolution interne"}, {"Shadow Resolution", "Résolution des ombres"},
        {"Configure Controller", "Configurer la manette"}, {"Allow Background Input", "Autoriser les entrées en arrière-plan"},
        {"Are you sure you want to quit?", "Voulez-vous vraiment quitter ?"},
        {"Yes", "Oui"}, {"No", "Non"},
    };
    for (const auto &entry : entries) {
        if (entry.source == text) {
            return std::string(entry.translated);
        }
    }
    return std::string(text);
}

} // namespace sms::ui
