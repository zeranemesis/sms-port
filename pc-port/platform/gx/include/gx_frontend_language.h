#ifndef GX_FRONTEND_LANGUAGE_H
#define GX_FRONTEND_LANGUAGE_H

#include <cstdlib>
#include <cstring>

// UI language follows the persisted SMS_LANGUAGE setting. The game reads
// this same setting in OSGetLanguage at boot, so changing it needs a restart.
namespace sms_frontend {
inline bool french()
{
    const char* code = std::getenv("SMS_LANGUAGE");
    return code && (code[0] == 'f' || code[0] == 'F') &&
           (code[1] == 'r' || code[1] == 'R');
}
inline const char* text(const char* english, const char* frenchText)
{
    return french() ? frenchText : english;
}
inline const char* languageCode(int index)
{
    static const char* codes[] = {"en", "de", "fr", "es", "it"};
    return codes[index >= 0 && index < 5 ? index : 0];
}
inline int languageIndex(const char* code)
{
    if (code)
        for (int i = 0; i < 5; ++i)
            if (std::strcmp(code, languageCode(i)) == 0) return i;
    return 0;
}
// Stable lookup strings: ImGui control identifiers must stay untranslated.
// Call this for visible text only, leaving settings keys and ## IDs intact.
inline const char* translate(const char* english)
{
    if (!english || !french()) return english;
    struct Entry { const char* en; const char* fr; };
    static const Entry entries[] = {
        {"Install", "Installation"}, {"Display", "Affichage"},
        {"Graphics", "Graphismes"}, {"Camera", "Caméra"},
        {"Gameplay", "Jeu"}, {"Audio", "Audio"},
        {"Controls", "Commandes"}, {"About", "À propos"},
        {"Settings", "Paramètres"}, {"Language", "Langue"},
        {"English", "Anglais"}, {"French", "Français"},
        {"Window mode", "Mode de fenêtre"}, {"Monitor", "Écran"},
        {"Fullscreen resolution", "Résolution plein écran"},
        {"Window size", "Taille de fenêtre"}, {"Vertical sync", "Synchronisation verticale"},
        {"Widescreen", "Écran large"}, {"HUD position", "Position de l'interface"},
        {"Aspect ratio", "Format d'image"}, {"Scaling filter", "Filtre de mise à l'échelle"},
        {"Internal resolution", "Résolution interne"},
        {"Anti-aliasing (MSAA)", "Anticrénelage (MSAA)"},
        {"Anisotropic filtering", "Filtrage anisotrope"},
        {"HD texture pack", "Pack de textures HD"}, {"Use HD textures", "Utiliser les textures HD"},
        {"HD cutscenes", "Cinématiques HD"}, {"Use HD cutscenes", "Utiliser les cinématiques HD"},
        {"Frame rate", "Cadence d'images"}, {"Skip intro movies", "Passer les cinématiques d'introduction"},
        {"Performance overlay", "Afficher les performances"}, {"Game mod", "Mod du jeu"},
        {"Sound", "Son"}, {"Volume", "Volume"}, {"Disc image", "Image du disque"},
        {"Install method", "Méthode d'installation"},
        {"Invert horizontal (X)", "Inverser l'axe horizontal (X)"},
        {"Invert vertical (Y)", "Inverser l'axe vertical (Y)"},
        {"Free camera", "Caméra libre"}, {"Mouse look", "Caméra à la souris"},
        {"Show this menu at startup", "Afficher ce menu au démarrage"},
        {"Ready to play", "Prêt à jouer"}, {"Game not installed", "Jeu non installé"},
        {"Browse...", "Parcourir..."}, {"Play", "Jouer"}, {"Resume", "Reprendre"},
        {"Quit", "Quitter"}, {"Back", "Retour"}, {"Cancel", "Annuler"},
        {"Save", "Enregistrer"}, {"Apply", "Appliquer"},
        {"Off", "Désactivé"}, {"On", "Activé"}, {"None", "Aucun"},
        {"Windowed", "Fenêtré"}, {"Borderless", "Sans bordure"}, {"Fullscreen", "Plein écran"},
        {"Achievements", "Succès"}, {"Updates", "Mises à jour"},
        {"Check for updates", "Vérifier les mises à jour"},
        {"Restart required", "Redémarrage nécessaire"},
        {"No mods found in mods/. See mods/README.md.", "Aucun mod dans mods/. Voir mods/README.md."}
    };
    for (const Entry& entry : entries)
        if (std::strcmp(english, entry.en) == 0) return entry.fr;
    return english;
}
}

#endif
