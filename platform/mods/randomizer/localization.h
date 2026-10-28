#pragma once

#include <cstdint>
#include <cstdio>

namespace sms_randomizer {

// Same indices as the game's PAL language option 0xA0001.
inline unsigned language_index(unsigned language) { return language < 5 ? language : 0; }

inline constexpr const char* ability_names[5][23] = {
    {"DOUBLE JUMP", "TRIPLE JUMP", "SIDEFLIP", "GRAB", "GROUND SPIN", "SPIN JUMP",
     "ROLLOUT", "WALL KICKS", "GROUND POUND", "Y CAM", "TALKING", "SHINE SHIRT",
     "SUNGLASSES", "HELMET", "YOSHI", "BLOOPER", "ROLLERCOASTER CART", "SPRAY NOZZLE",
     "SPAM SPRAY", "HOVER NOZZLE", "ROCKET NOZZLE", "TURBO NOZZLE", "CLIMBING"},
    {"DOPPELSPRUNG", "DREIFACHSPRUNG", "SALTO", "GREIFEN", "BODENWIRBEL", "WIRBELSPRUNG",
     "HECHTSPRUNGROLLE", "WANDSPRUNG", "STAMPFATTACKE", "Y-KAMERA", "GESPRÄCHE", "INSIGNIEN-HEMD",
     "SONNENBRILLE", "HELM", "YOSHI", "BLOOPER", "ACHTERBAHNWAGEN", "SPRITZDÜSE",
     "SCHNELLES SPRITZEN", "SCHWEBEDÜSE", "RAKETENDÜSE", "TURBODÜSE", "KLETTERN"},
    {"DOUBLE SAUT", "TRIPLE SAUT", "SAUT PÉRILLEUX", "PRISE D'OBJETS", "ROTATION AU SOL", "SAUT TOURBILLON",
     "REBOND APRÈS PLONGEON", "SAUT MURAL", "ATTAQUE AU SOL", "CAMÉRA Y", "DIALOGUES", "CHEMISE SOLEIL",
     "LUNETTES", "CASQUE", "YOSHI", "BLOUP", "WAGONNET", "BUSE ARROSAGE", "ARROSAGE RAPIDE",
     "BUSE STATIONNAIRE", "BUSE PROPULSION", "BUSE TURBO", "ESCALADE"},
    {"SALTO DOBLE", "SALTO TRIPLE", "SALTO LATERAL", "AGARRAR", "GIRO EN EL SUELO", "SALTO GIRATORIO",
     "REBOTE TRAS ZAMBULLIDA", "SALTO EN PARED", "SALTO BOMBA", "CÁMARA Y", "DIÁLOGOS", "CAMISETA SOL",
     "GAFAS DE SOL", "CASCO", "YOSHI", "BLOOPER", "VAGÓN DE MONTAÑA RUSA", "BOQUILLA DE CHORRO",
     "CHORRO RÁPIDO", "BOQUILLA DEL PLANEADOR", "BOQUILLA DEL PROPULSOR", "BOQUILLA TURBO", "ESCALADA"},
    {"DOPPIO SALTO", "TRIPLO SALTO", "SALTO LATERALE", "PRESA", "ROTAZIONE A TERRA", "SALTO ROTANTE",
     "RIMBALZO DOPO IL TUFFO", "SALTO A MURO", "SCHIANTO A TERRA", "TELECAMERA Y", "DIALOGHI", "CAMICIA SOLE",
     "OCCHIALI DA SOLE", "CASCO", "YOSHI", "CALAMAKO", "VAGONE DELLE MONTAGNE RUSSE", "SPRUZZATORE",
     "SPRUZZO RAPIDO", "SPRUZZOPLANO", "SPRUZZORAZZO", "SPRUZZOTURBO", "ARRAMPICATA"},
};

inline const char* unlock_name(std::uint32_t reward, unsigned language = 0)
{
    for (unsigned bit = 0; bit < 23; ++bit)
        if (reward == (1u << bit)) return ability_names[language_index(language)][bit];
    return nullptr;
}

enum class Text : unsigned {
    Description, Creator, Engine, Project, Classic, Abilities, NoAbilities, Preview,
    Seed, Enable, Disable, Restart, Nozzles, Hover, Rocket, Turbo, NoNozzles,
    PCOnly, InvalidSeed, SeedBounds, Saved, SaveFailed, NozzleReady, Unlocked,
    NextLaunch, Enabled, Disabled, EntrancePreview, Count
};

inline constexpr const char* translations[static_cast<unsigned>(Text::Count)][5] = {
    {"TheAzack9's classic randomizer: shuffled level entrances and abilities to unlock as you play.",
     "Der klassische Randomizer von TheAzack9: zufällige Level-Eingänge und Fähigkeiten zum Freischalten.",
     "Le randomizer classique de TheAzack9 : entrées des niveaux et capacités à débloquer au fil de la partie.",
     "El randomizer clásico de TheAzack9: entradas de niveles aleatorias y habilidades para desbloquear.",
     "Il randomizer classico di TheAzack9: ingressi dei livelli casuali e abilità da sbloccare durante il gioco."},
    {"Creator", "Ersteller", "Créateur", "Creador", "Creatore"},
    {"Engine: BetterSunshineEngine and its contributors.", "Engine: BetterSunshineEngine und Mitwirkende.",
     "Moteur : BetterSunshineEngine et ses contributeurs.", "Motor: BetterSunshineEngine y sus colaboradores.",
     "Motore: BetterSunshineEngine e i suoi collaboratori."},
    {"Project", "Projekt", "Projet", "Proyecto", "Progetto"},
    {"Classic integration test: original beta 0.3.6 generator and progressive abilities. Abilities unlocked: %u / 23. Special transitions and goals are still being ported.",
     "Klassischer Integrationstest: originaler Beta-0.3.6-Generator und freischaltbare Fähigkeiten. Freigeschaltet: %u / 23. Besondere Übergänge und Ziele werden noch portiert.",
     "Intégration classique en test : générateur original bêta 0.3.6 et capacités progressives. Capacités obtenues : %u / 23. Les transitions spéciales et les objectifs restent en cours de portage.",
     "Prueba de integración clásica: generador original beta 0.3.6 y habilidades progresivas. Habilidades desbloqueadas: %u / 23. Las transiciones especiales y los objetivos aún se están adaptando.",
     "Test di integrazione classica: generatore originale beta 0.3.6 e abilità progressive. Abilità sbloccate: %u / 23. Le transizioni speciali e gli obiettivi sono ancora in fase di adattamento."},
    {"Unlocked abilities", "Freigeschaltete Fähigkeiten", "Capacités obtenues", "Habilidades desbloqueadas", "Abilità sbloccate"},
    {"No abilities unlocked yet.", "Noch keine Fähigkeiten freigeschaltet.", "Aucune capacité obtenue pour le moment.",
     "Aún no has desbloqueado ninguna habilidad.", "Nessuna abilità sbloccata per il momento."},
    {"Native preview: shuffled level entrances, all abilities available from the start. These seeds differ from the console mod.",
     "Native Vorschau: zufällige Level-Eingänge, alle Fähigkeiten von Anfang an verfügbar. Die Seeds unterscheiden sich vom Konsolen-Mod.",
     "Préversion native : entrées des niveaux mélangées, capacités disponibles dès le départ. Les seeds de cette préversion diffèrent de celles du mod console.",
     "Versión preliminar nativa: entradas de niveles aleatorias y todas las habilidades disponibles desde el inicio. Estas semillas difieren de las del mod de consola.",
     "Anteprima nativa: ingressi dei livelli casuali e tutte le abilità disponibili dall'inizio. Questi seed sono diversi da quelli della mod per console."},
    {"Seed (0 to 4294967295)", "Seed (0 bis 4294967295)", "Seed (0 à 4294967295)", "Semilla (0 a 4294967295)", "Seed (da 0 a 4294967295)"},
    {"Enable on next launch", "Beim nächsten Start aktivieren", "Activer au prochain démarrage", "Activar en el próximo inicio", "Attiva al prossimo avvio"},
    {"Normal game on next launch", "Normales Spiel beim nächsten Start", "Jeu normal au prochain démarrage", "Juego normal en el próximo inicio", "Gioco normale al prossimo avvio"},
    {"Each seed uses a separate memory card. Restart with Jouer-Aurora.cmd to apply this choice.",
     "Jeder Seed verwendet eine eigene Speicherkarte. Zum Anwenden mit Jouer-Aurora.cmd neu starten.",
     "Une carte séparée est créée pour chaque seed. Relancez avec Jouer-Aurora.cmd pour appliquer ce choix.",
     "Cada semilla usa una tarjeta de memoria independiente. Reinicia con Jouer-Aurora.cmd para aplicar esta opción.",
     "Ogni seed usa una scheda di memoria separata. Riavvia con Jouer-Aurora.cmd per applicare la scelta."},
    {"Available backup nozzles", "Verfügbare Zusatzdüsen", "Buses de secours disponibles", "Boquillas secundarias disponibles", "Ugelli secondari disponibili"},
    {"Hover", "Schwebedüse", "Stationnaire", "Planeador", "Spruzzoplano"},
    {"Rocket", "Raketendüse", "Propulsion", "Propulsor", "Spruzzorazzo"},
    {"Turbo", "Turbodüse", "Turbo", "Turbo", "Spruzzoturbo"},
    {"No backup nozzles unlocked yet.", "Noch keine Zusatzdüsen freigeschaltet.", "Aucune buse de secours obtenue pour le moment.",
     "Aún no has desbloqueado ninguna boquilla secundaria.", "Nessun ugello secondario sbloccato per il momento."},
    {"This native preview is available on PC.", "Diese native Vorschau ist auf dem PC verfügbar.", "Cette préversion native est disponible sur PC.",
     "Esta versión preliminar nativa está disponible en PC.", "Questa anteprima nativa è disponibile su PC."},
    {"Enter an integer seed between 0 and 4294967295.", "Einen ganzzahligen Seed zwischen 0 und 4294967295 eingeben.",
     "Entrez une seed entière entre 0 et 4294967295.", "Introduce una semilla entera entre 0 y 4294967295.", "Inserisci un seed intero tra 0 e 4294967295."},
    {"Seed out of range.", "Seed außerhalb des gültigen Bereichs.", "Seed hors limites.", "Semilla fuera de rango.", "Seed fuori intervallo."},
    {"Choice saved. Close the game and restart Jouer-Aurora.cmd.", "Auswahl gespeichert. Das Spiel schließen und Jouer-Aurora.cmd neu starten.",
     "Choix enregistré. Fermez le jeu puis relancez Jouer-Aurora.cmd.", "Opción guardada. Cierra el juego y reinicia Jouer-Aurora.cmd.",
     "Scelta salvata. Chiudi il gioco e riavvia Jouer-Aurora.cmd."},
    {"Could not save the choice.", "Die Auswahl konnte nicht gespeichert werden.", "Impossible d'enregistrer le choix.", "No se ha podido guardar la opción.", "Impossibile salvare la scelta."},
    {"Nozzle ready: resume the game to use it.", "Düse bereit: Zum Verwenden das Spiel fortsetzen.", "Buse prête : reprenez la partie pour l'utiliser.",
     "Boquilla lista: reanuda la partida para usarla.", "Ugello pronto: riprendi la partita per usarlo."},
    {"UNLOCKED: %s", "FREIGESCHALTET: %s", "DÉBLOQUÉ : %s", "DESBLOQUEADO: %s", "SBLOCCATO: %s"},
    {"Next launch", "Nächster Start", "Prochain lancement", "Próximo inicio", "Prossimo avvio"},
    {"Classic Randomizer enabled", "Klassischer Randomizer aktiviert", "Randomizer classique activé", "Randomizer clásico activado", "Randomizer classico attivato"},
    {"Disabled: normal game", "Deaktiviert: normales Spiel", "Désactivé : jeu normal", "Desactivado: juego normal", "Disattivato: gioco normale"},
    {"Entrances preview", "Vorschau der Level-Eingänge", "Préversion des entrées", "Versión preliminar de entradas", "Anteprima degli ingressi"},
};

inline const char* text(Text id, unsigned language)
{
    const auto index = static_cast<unsigned>(id);
    return index < static_cast<unsigned>(Text::Count) ? translations[index][language_index(language)] : "";
}

// The PAL game font consumes single-byte Western text; RmlUi consumes UTF-8.
inline bool reward_message(std::uint32_t reward, unsigned language, char* output, unsigned capacity)
{
    if (!output || !capacity) return false;
    output[0] = 0;
    const char* name = unlock_name(reward, language);
    if (!name) return false;
    char utf8[256];
    const int size = std::snprintf(utf8, sizeof(utf8), text(Text::Unlocked, language), name);
    if (size < 0 || static_cast<unsigned>(size) >= sizeof(utf8)) return false;
    unsigned written = 0;
    for (unsigned i = 0; i < static_cast<unsigned>(size); ++i) {
        unsigned char ch = static_cast<unsigned char>(utf8[i]);
        if (ch >= 0x80) {
            if ((ch != 0xc2 && ch != 0xc3) || i + 1 >= static_cast<unsigned>(size)) return false;
            const unsigned char next = static_cast<unsigned char>(utf8[++i]);
            if ((next & 0xc0) != 0x80) return false;
            ch = static_cast<unsigned char>(((ch & 3) << 6) | (next & 0x3f));
        }
        if (written + 1 >= capacity) { output[0] = 0; return false; }
        output[written++] = static_cast<char>(ch);
    }
    output[written] = 0;
    return true;
}

} // namespace sms_randomizer
