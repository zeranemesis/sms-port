#include "partyboard_menu.h"
#include "pc_services.h"
#include "gamebanana.h"
#include "retroachievements.h"
#include <RmlUi/Core/Elements/ElementFormControl.h>
#include <RmlUi/Core.h>
#include <RmlUi/Core/Input.h>
#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <utility>

namespace sms_frontend {
namespace {
std::string escape(const std::string& value) {
    std::string result;
    for(char c:value){if(c=='&')result+="&amp;";else if(c=='<')result+="&lt;";else if(c=='>')result+="&gt;";else if(c=='\"')result+="&quot;";else if(c=='\'')result+="&#39;";else result+=c;}
    return result;
}
struct Choice {std::string value,label;};
struct Row {
    std::string key,label,help,initial;
    std::vector<Choice> choices;
    int binding = -1;
    int minimum = 0, maximum = 100, step = 1;
    bool numeric = false;
    Row() {}
    Row(const char* k,const char* l,const char* h,const char* d,std::initializer_list<Choice> c)
        : key(k),label(l),help(h),initial(d),choices(c) {}
};
const char* tab_titles[]={"Vidéo et graphismes","Audio","Manettes","Jeu","Sauvegardes","Mods","Succès","Mises à jour"};
const char* default_labels[]={"Bouton A","Bouton B","Bouton X","Bouton Y","Bouton Z","Gâchette L","Gâchette R","Start",
    "Croix haut","Croix bas","Croix gauche","Croix droite","Stick haut","Stick bas","Stick gauche","Stick droite",
    "Stick C haut","Stick C bas","Stick C gauche","Stick C droite","Marcher","Quitter"};
const char* default_keys[]={"SPACE X","LSHIFT RSHIFT C","V","F","Z","Q","E","ENTER","1 KP_8","2 KP_2","3 KP_4","4 KP_6",
    "UP W","DOWN S","LEFT A","RIGHT D","I","K","J","L","LCTRL","ESCAPE"};
Row toggle(const char* key,const char* label,const char* help,bool initial) {
    return Row(key,label,help,initial?"on":"off",{{"off","Désactivé"},{"on","Activé"}});
}
Row number(const char* key,const char* label,const char* help,int initial,int min,int max,int step) {
    Row r(key,label,help,std::to_string(initial).c_str(),{});r.numeric=true;r.minimum=min;r.maximum=max;r.step=step;return r;
}
} // namespace

struct PartyBoardMenu::Impl : Rml::EventListener {
    Rml::Context* context;
    Rml::ElementDocument* document = nullptr;
    MenuBindings bindings;
    bool shown=false;
    int page=0,selected=0,capture=-1;
    bool add_key=false;
    bool fidelity_menu=false;
    std::vector<Row> rows;
    std::vector<std::string> backups;
    std::string status,last_update_signature,last_service_signature,last_install;
    Impl(Rml::Context* c,const MenuBindings& b):context(c),bindings(b) {
        if(!context)return;
        const std::string root=bindings.resourceDirectory;
        Rml::LoadFontFace(root+"/Inter-Regular.ttf",true);
        Rml::LoadFontFace(root+"/FOT-NewRodin Pro DB.otf");
        Rml::LoadFontFace(root+"/AlegreyaSC-Regular.ttf");
        Rml::LoadFontFace(root+"/AlegreyaSC-Bold.ttf");
        Rml::LoadFontFace(root+"/MaterialSymbolsRounded-Regular.ttf");
        std::string markup="<rml><head><title>Super Mario Sunshine — PartyBoard</title>"
            "<link type='text/rcss' href='window.rcss'/><link type='text/rcss' href='tabbing.rcss'/>"
            "<link type='text/rcss' href='sunshine.rcss'/></head><body><window id='settings-window' open>"
            "<tab-bar id='tabs' closable>";
        for(int i=0;i<8;++i)markup+="<tab id='tab-"+std::to_string(i)+"' data-action='tab' data-index='"+std::to_string(i)+"' tabindex='0'>"+tab_titles[i]+"</tab>";
        markup+="<tab-end-spacer/><close id='close' data-action='resume' tabindex='0'/></tab-bar>"
            "<content><pane id='options'/><pane id='detail'/></content>"
            "<div id='status-line'/><footer><button id='resume' data-action='resume'>Reprendre</button>"
            "<button id='save' data-action='save'>Enregistrer les réglages</button>"
            "<button id='quit' data-action='quit'>Quitter</button></footer></window></body></rml>";
        document=context->LoadDocumentFromMemory(markup,root+"/sunshine.rml");
        if(!document)return;
        document->AddEventListener("click",this);
        document->AddEventListener("keydown",this,true);
        document->AddEventListener("focus",this,true);
        document->AddEventListener("mousescroll",this);
        select_page(0,false);
        document->Hide();
    }
    ~Impl(){if(document){document->RemoveEventListener("click",this);document->RemoveEventListener("keydown",this,true);document->RemoveEventListener("focus",this,true);document->RemoveEventListener("mousescroll",this);document->Close();}}
    std::string value(const Row& row) const {
        if(row.binding>=0)return bindings.getBinding?bindings.getBinding(row.binding):default_keys[row.binding];
        return bindings.getSetting?bindings.getSetting(row.key.c_str(),row.initial.c_str()):row.initial;
    }
    std::string label_value(const Row& row) const {
        std::string v=value(row);for(const Choice& c:row.choices)if(c.value==v)return c.label;return v;
    }
    void add_info(const char* key,const char* label,const char* help){rows.push_back(Row(key,label,help,"",{}));}
    void make_rows() {
        rows.clear();
        if(page==0 && fidelity_menu){
            add_info("fidelity-original","Fidélité GameCube","Résolution 1×, format 4:3, 30 images/s, textures originales, sans MSAA, FXAA, anisotropie ni renforcement de netteté. Les calculs de couleur du jeu sont conservés.");
            add_info("fidelity-enhanced","Original en haute résolution","Conserve les textures, couleurs et cadence d’origine, avec une résolution interne 3×.");
            add_info("fidelity-back","Retour aux réglages","Afficher les réglages vidéo et graphiques.");
            return;
        }
        switch(page){
        case 0:
            add_info("fidelity","Fidélité au jeu d’origine","Choisir un profil inspiré du rendu GameCube ou du rendu original en haute résolution.");
            rows.push_back(Row("window_mode","Mode de fenêtre","F11 ou Alt+Entrée bascule le plein écran pendant le jeu.","windowed",{{"windowed","Fenêtré"},{"borderless","Sans bordure"},{"fullscreen","Plein écran exclusif"}}));
            rows.push_back(Row("window_scale","Taille de fenêtre","Taille de départ ; la fenêtre reste redimensionnable.","0",{{"0","Automatique"},{"1","640 × 480"},{"2","1280 × 960"},{"3","1920 × 1440"},{"4","2560 × 1920"}}));
            rows.push_back(Row("vsync","Synchronisation verticale","Évite le déchirement. Le mode adaptatif autorise une image tardive.","off",{{"off","Désactivée"},{"on","Activée"},{"adaptive","Adaptative"}}));
            rows.push_back(Row("widescreen","Écran large","Affiche davantage du monde sans étirer les menus d'origine.","off",{{"off","4:3 d'origine"},{"16:9","16:9"},{"16:10","16:10"},{"21:9","21:9"},{"32:9","32:9"}}));
            rows.push_back(Row("widescreen_hud","Position des compteurs","Position du HUD en écran large.","centre",{{"centre","Centre"},{"edges","Bords de l'écran"}}));
            rows.push_back(Row("aspect","Rapport d'image","Conserve les proportions, remplit la fenêtre ou utilise des multiples entiers.","keep",{{"keep","Conserver"},{"stretch","Étirer"},{"integer","Facteur entier"}}));
            rows.push_back(Row("present_filter","Filtre de présentation","Filtrage de l'image finale dans la fenêtre.","bilinear",{{"bilinear","Lisse"},{"nearest","Pixels bruts"}}));
            rows.push_back(Row("resolution","Résolution interne","Multiples de l'image GameCube 640 × 528. Les grandes valeurs sollicitent davantage le GPU.","1",{{"1","1× · 640 × 528"},{"2","2× · 1280 × 1056"},{"3","3× · 1920 × 1584"},{"4","4× · 2560 × 2112"},{"5","5× · 3200 × 2640"},{"6","6× · 3840 × 3168"},{"7","7× · 4480 × 3696"},{"8","8× · 5120 × 4224"}}));
            rows.push_back(Row("msaa","Anticrénelage MSAA","Lisse les contours des polygones.","0",{{"0","Désactivé"},{"2","2×"},{"4","4×"},{"8","8×"}}));
            rows.push_back(toggle("fxaa","FXAA","Lissage des contours par post-traitement.",false));
            rows.push_back(Row("anisotropic","Filtrage anisotrope","Améliore les textures vues en biais.","0",{{"0","Désactivé"},{"2","2×"},{"4","4×"},{"8","8×"},{"16","16×"}}));
            rows.push_back(Row("brightness","Luminosité","1,00 correspond à l'image d'origine.","1.0",{{"0.5","0,50"},{"0.75","0,75"},{"1.0","1,00"},{"1.25","1,25"},{"1.5","1,50"},{"2.0","2,00"}}));
            break;
        case 1:
            rows.push_back(toggle("audio","Son","Active ou coupe la sortie audio.",true));
            rows.push_back(number("volume","Volume général","Volume de sortie, de 0 à 100 %.",100,0,100,5));
            break;
        case 2:
            for(int i=0;i<22;++i){Row r; r.key="binding-"+std::to_string(i);r.label=i<(int)bindings.bindingLabels.size()?bindings.bindingLabels[i]:default_labels[i];r.binding=i;
                r.help="Sélectionnez Remplacer ou Ajouter, puis appuyez sur une touche. Échap annule la capture. Les manettes SDL sont reconnues automatiquement.";rows.push_back(r);}
            rows.push_back(toggle("camera_invert_x","Inverser la caméra horizontalement","Inverse le stick droit et la souris.",false));
            rows.push_back(toggle("camera_invert_y","Inverser la caméra verticalement","Inverse le stick droit et la souris.",false));
            rows.push_back(toggle("free_camera","Caméra libre","La caméra conserve son orientation. L permet de la recentrer.",false));
            rows.push_back(number("camera_speed","Vitesse de caméra","100 % correspond à la vitesse d'origine.",100,25,300,5));
            rows.push_back(toggle("mouse_camera","Caméra à la souris","F10 libère la souris ; un clic dans le jeu la capture à nouveau.",false));
            rows.push_back(number("mouse_sensitivity","Sensibilité de la souris","Sensibilité de déplacement de la caméra.",100,10,500,5));
            break;
        case 3:
            rows.push_back(Row("language","Langue du jeu","La langue est utilisée au prochain chargement de niveau, sans relancer le jeu. Les textes déjà chargés sont conservés.","en",{{"en","English"},{"fr","Français"},{"de","Deutsch"},{"es","Español"},{"it","Italiano"}}));
            rows.push_back(Row("frame_rate","Cadence du jeu","Les menus et cinématiques conservent leur cadence d'origine.","30",{{"30","30 images/s · original"},{"60","60 images/s"}}));
            rows.push_back(toggle("skip_movies","Passer les cinématiques","Évite les films d'introduction.",false));
            rows.push_back(toggle("overlay","Mesures de performance","Affiche les temps et la cadence du rendu.",false));
            rows.push_back(toggle("menu_start","Menu au démarrage","Ouvre le menu PartyBoard à la première image du jeu.",false));
            break;
        case 4:
            add_info("backup","Créer une sauvegarde","Copie de la carte actuelle dans un nouveau dossier. Le jeu doit être suspendu pendant cette opération.");
            add_info("restore","Restaurer une sauvegarde","Prépare une nouvelle carte pour le prochain démarrage. La carte actuelle reste intacte.");
            add_info("card-path","Dossier de sauvegarde","La carte active utilise le dossier ci-dessous.");backups=save_backups();break;
        case 5:{
            if (std::getenv("SMS_CUBESHELF") && std::string(std::getenv("SMS_CUBESHELF"))=="1") {
                add_info("cubeshelf-mods","Mods gérés par CubeShelf","Téléchargez, activez et organisez vos mods dans CubeShelf. Les choix sont chargés au lancement du jeu. Les réglages graphiques, audio et manettes restent disponibles ici.");
                rows.push_back(toggle("texture_packs","Textures HD","Autorise les textures des mods sélectionnés dans CubeShelf.",true));
                break;
            }
            add_info("installed-mods","Mods installés","Activez ou désactivez vos mods. Les fichiers changent au prochain chargement de niveau ; les textures se rechargent en direct.");
            add_info("gamebanana","Télécharger des mods","Catalogue GameBanana : choisissez un mod puis une archive à télécharger et installer.");
            rows.push_back(toggle("texture_packs","Textures HD","Autorise les packs de textures actifs.",true));
            rows.push_back(toggle("hd_cutscenes","Cinématiques HD","Utilise les films HD installés.",true));break;}
        case 6:
            add_info("ra-login","Compte RetroAchievements","Connexion au compte RetroAchievements. Le mot de passe est envoyé au service et n’est jamais enregistré.");
            add_info("achievements","Succès RetroAchievements","Liste officielle du jeu 6049. Le déblocage nécessite une traduction validée de la mémoire GameCube du jeu vers le port natif.");break;
        case 7:
            add_info("updates","Mise à jour du port","Releases Windows 64 PAL du dépôt officiel. Archives vérifiées par SHA-256, installation après fermeture du jeu.");break;
        }
    }
    Rml::Element* element(const char* id){return document?document->GetElementById(id):nullptr;}
    void status_text(const std::string& text){status=text;if(auto* e=element("status-line"))e->SetInnerRML(escape(text));}
    void render_rows() {
        std::string html="<div class='section-heading'>"+std::string(tab_titles[page])+"</div>";
        for(size_t i=0;i<rows.size();++i){const Row& row=rows[i];html+="<select-button id='row-"+std::to_string(i)+"' data-action='row' data-index='"+std::to_string(i)+"' tabindex='0'><key>"+escape(row.label)+"</key><value>"+escape(label_value(row))+"</value></select-button>";}
        html+="<spacer/>";
        if(auto* e=element("options"))e->SetInnerRML(html);
        for(size_t i=0;i<rows.size();++i)if(auto* e=element(("row-"+std::to_string(i)).c_str()))e->SetPseudoClass("selected",(int)i==selected);
    }
    std::string button(const std::string& action,const std::string& label,const std::string& value="") {
        return "<button data-action='"+escape(action)+"' data-value='"+escape(value)+"'>"+escape(label)+"</button>";
    }
    std::vector<std::string> active_mods() const {
        std::vector<std::string> result;std::string item;
        const std::string setting=bindings.getSetting?bindings.getSetting("mod","none"):"none";
        for(char c:setting+";"){if(c==';'||c==','){if(!item.empty()&&item!="none"&&item!="0")result.push_back(item);item.clear();}else item+=c;}return result;
    }
    std::string installed_card(const InstalledMod& m) {
        const auto active=active_mods();const bool on=m.textures?m.enabled:std::find(active.begin(),active.end(),m.id)!=active.end();
        return "<div class='mod-card'><div class='mod-title'>"+escape(m.name)+"</div><div class='help'>"+(m.textures?"Textures":m.mixed?"Fichiers et textures":"Fichiers du jeu")+" · Installé · "+(on?"Activé":"Désactivé")+"</div>"+button("mod-toggle",on?"Désactiver":"Activer",m.id)+"</div>";
    }
    void render_detail() {
        if(selected<0||selected>=(int)rows.size())return;
        const Row& row=rows[selected];std::string html="<div class='section-heading'>"+escape(row.label)+"</div><div class='help'>"+escape(row.help)+"</div>";
        if(row.binding>=0){
            html+="<div class='current-binding'>"+escape(value(row))+"</div>";
            if(capture>=0)html+="<div class='capture'>Appuyez sur une touche…</div>"+button("cancel-capture","Annuler");
            else html+=button("capture","Remplacer")+button("add-key","Ajouter une touche")+button("reset-binding","Rétablir les touches d'origine");
        }else if(row.numeric){html+="<div class='numeric-value'>"+escape(value(row))+" %</div><div class='button-row'>"+button("decrease","−")+button("increase","+")+"</div>";
        }else if(!row.choices.empty()){
            for(size_t i=0;i<row.choices.size();++i){const Choice& c=row.choices[i];html+="<button id='choice-"+std::to_string(i)+"' data-action='choice' data-value='"+escape(c.value)+"'>"+escape(c.label)+"</button>";}
        }else if(row.key=="fidelity")html+=button("fidelity-open","Ouvrir les profils de fidélité");
        else if(row.key=="fidelity-original"||row.key=="fidelity-enhanced")html+=button(row.key,"Appliquer ce profil");
        else if(row.key=="fidelity-back")html+=button("fidelity-back","Retour");
        else if(row.key=="backup")html+=button("backup","Créer une copie de la carte");
        else if(row.key=="restore"){
            html+=button("refresh-backups","Actualiser la liste");
            if(backups.empty())html+="<div>Aucune sauvegarde disponible.</div>";
            for(size_t i=0;i<backups.size();++i){std::string p=backups[i];size_t slash=p.find_last_of("/\\");html+=button("restore","Restaurer · "+p.substr(slash==std::string::npos?0:slash+1),std::to_string(i));}
        }else if(row.key=="card-path")html+="<div>"+escape(save_directory())+"</div>";
        else if(row.key=="ra-login"){
            html+="<div id='ra-status'>"+escape(ra::status())+"</div>";
            html+="<div>Nom d’utilisateur</div><input id='ra-user' type='text' maxlength='64' value='"+escape(ra::username())+"'/>";
            html+="<div>Mot de passe</div><input id='ra-password' type='password' maxlength='256'/>";
            html+=button("ra-login","Se connecter")+button("ra-logout","Se déconnecter");
        }else if(row.key=="achievements"){
            html+="<div>"+escape(ra::status())+"</div>";
            for(const ra::Achievement& a:ra::achievements())html+="<div class='achievement "+std::string(a.unlocked?"unlocked":"locked")+"'><div>"+escape(a.title)+" · "+(a.unlocked?"Débloqué":"Verrouillé")+"</div><div class='help'>"+escape(a.description)+"</div></div>";
        }else if(row.key=="installed-mods"){
            html+=button("mods-refresh","Actualiser les mods installés");
            const auto mods=gamebanana_installed();
            if(mods.empty())html+="<div>Aucun mod installé. Ouvrez Télécharger des mods.</div>";
            for(const auto& m:mods)html+=installed_card(m);
        }else if(row.key=="gamebanana"){
            const BananaStatus state=gamebanana_status();const auto installed=gamebanana_installed();
            html+="<div class='mod-status'>"+escape(state.message)+"</div>";
            if(state.state==BananaState::Loading||state.state==BananaState::Downloading)html+="<div>Opération en cours...</div>";
            else{
                html+=button("banana-refresh","Actualiser le catalogue");
                if(state.selected_mod){
                    html+=button("banana-page","Retour au catalogue",std::to_string(state.page));
                    if(state.files.empty()&&state.state==BananaState::Ready)html+="<div>Aucune archive disponible.</div>";
                    for(const auto& file:state.files){
                        const std::string key="gamebanana-"+std::to_string(state.selected_mod)+"-"+std::to_string(file.id);
                        const auto found=std::find_if(installed.begin(),installed.end(),[&](const InstalledMod& m){return m.id==key||m.id=="textures/"+key;});
                        if(found!=installed.end()){html+=installed_card(*found);continue;}
                        html+="<div class='mod-card'><div class='mod-title'>"+escape(file.name)+"</div><div class='help'>Non installé · "+std::to_string(file.bytes/1024)+" Ko</div>";
                        if(file.zip)html+=button("banana-install","Télécharger et installer",std::to_string(file.id));
                        else html+="<div>Format nécessitant une prise en charge spécifique.</div>";
                        html+="</div>";
                    }
                }else{
                    html+="<div class='help'>Page "+std::to_string(state.page)+" · "+std::to_string(state.total)+" mods</div><div class='button-row'>";
                    if(state.page>1)html+=button("banana-page","Précédente",std::to_string(state.page-1));
                    if(state.page*20<state.total)html+=button("banana-page","Suivante",std::to_string(state.page+1));html+="</div>";
                    for(const auto& mod:state.mods){
                        const std::string prefix="gamebanana-"+std::to_string(mod.id)+"-";
                        const bool present=std::any_of(installed.begin(),installed.end(),[&](const InstalledMod& m){return m.id.compare(0,prefix.size(),prefix)==0||m.id.compare(0,9+prefix.size(),"textures/"+prefix)==0;});
                        html+="<div class='mod-card'><div class='mod-title'>"+escape(mod.name)+"</div><div class='help'>"+escape(mod.author)+" · "+escape(mod.category)+" · "+(present?"Installé":"Non installé")+"</div>"+button("banana-files",present?"Gérer / voir les fichiers":"Voir les téléchargements",std::to_string(mod.id))+"</div>";
                    }
                }
            }
        }else if(row.key=="updates"){
            UpdateStatus u=update_status();html+="<div>"+escape(u.message)+"</div>";
            if(!u.version.empty())html+="<div>Version : "+escape(u.version)+"</div>";
            if(u.state!=UpdateState::Checking && u.state!=UpdateState::Downloading)html+=button("check-update","Vérifier les mises à jour");
            if(u.state==UpdateState::Available)html+=button("download-update","Télécharger et vérifier");
            if(u.state==UpdateState::Ready)html+=button("install-update","Installer après fermeture et redémarrer")+"<div class='help'>"+escape(u.staged_path)+"</div>";
        }
        html+="<spacer/>";if(auto* e=element("detail"))e->SetInnerRML(html);
        for(size_t i=0;i<row.choices.size();++i)if(auto* e=element(("choice-"+std::to_string(i)).c_str()))e->SetPseudoClass("selected",row.choices[i].value==value(row));
    }
    void select_page(int next,bool focus) {
        page=(next+8)%8;fidelity_menu=false;selected=0;capture=-1;if(bindings.refreshMods)bindings.availableMods=bindings.refreshMods();make_rows();render_rows();render_detail();
        for(int i=0;i<8;++i)if(auto* e=element(("tab-"+std::to_string(i)).c_str()))e->SetPseudoClass("selected",i==page);
        if(focus)if(auto* e=element(("tab-"+std::to_string(page)).c_str())){e->Focus();e->ScrollIntoView();}
    }
    void select_row(int next,bool focus) {
        if(rows.empty())return;
        selected=(next+(int)rows.size())%(int)rows.size();capture=-1;
        for(size_t i=0;i<rows.size();++i)if(auto* e=element(("row-"+std::to_string(i)).c_str()))e->SetPseudoClass("selected",(int)i==selected);
        if(rows[selected].key=="gamebanana"&&gamebanana_status().state==BananaState::Idle)gamebanana_catalogue();
        render_detail();if(focus)if(auto* e=element(("row-"+std::to_string(selected)).c_str())){e->Focus();e->ScrollIntoView();}
    }
    void write_value(const std::string& v,const std::string& focus_action="") {
        if(selected<0||selected>=(int)rows.size()||!bindings.setSetting)return;
        const Row& row=rows[selected];bindings.setSetting(row.key.c_str(),v);
        if(auto* e=element(("row-"+std::to_string(selected)).c_str())) {
            auto* v_element=e->GetChild(1);if(v_element)v_element->SetInnerRML(escape(label_value(row)));
        }
        render_detail();
        for(size_t i=0;i<row.choices.size();++i)if(row.choices[i].value==v)
            if(auto* choice=element(("choice-"+std::to_string(i)).c_str()))choice->Focus();
        if(!focus_action.empty())if(auto* detail=element("detail")){Rml::ElementList buttons;detail->GetElementsByTagName(buttons,"button");
            for(auto* b:buttons)if(b->GetAttribute<std::string>("data-action","")==focus_action){b->Focus();break;}}
        const bool saved=bindings.onSave && bindings.onSave();
        status_text(!saved ? "Échec de l’application du réglage." : row.key=="mod" || row.key=="hd_cutscenes" ? "Choix enregistré pour le prochain chargement de niveau, sans redémarrage. Les fichiers de la scène actuelle sont conservés." : row.key=="language" ? "Langue sélectionnée pour le prochain chargement de niveau, sans redémarrage." : "Réglage enregistré et appliqué en direct.");
    }
    bool resume(){if(bindings.onResume && !bindings.onResume())return false;shown=false;capture=-1;document->Hide();return true;}
    void dispatch(const std::string& action,const std::string& val) {
        if(action=="fidelity-open"||action=="fidelity-back"){
            fidelity_menu=action=="fidelity-open";selected=0;make_rows();render_rows();render_detail();return;
        }
        if(action=="fidelity-original"||action=="fidelity-enhanced"){
            if(bindings.setSetting){
                const std::pair<const char*,const char*> settings[]={{"widescreen","off"},{"aspect","keep"},{"frame_rate","30"},{"msaa","0"},{"fxaa","off"},{"anisotropic","0"},{"sharpen","0"},{"brightness","1.0"},{"present_filter","bilinear"},{"texture_packs","off"},{"hd_cutscenes","off"}};
                for(const auto& setting:settings)bindings.setSetting(setting.first,setting.second);
                bindings.setSetting("resolution",action=="fidelity-original"?"1":"3");
                status_text(bindings.onSave&&bindings.onSave()?"Profil appliqué au rendu. Les textures déjà chargées par le jeu seront remplacées à leur prochain chargement.":"Échec de l’application du profil.");
            }return;
        }
        if(action=="ra-login"){
            auto* user=dynamic_cast<Rml::ElementFormControl*>(element("ra-user"));
            auto* password=dynamic_cast<Rml::ElementFormControl*>(element("ra-password"));
            if(user&&password){std::string secret=password->GetValue();password->SetValue("");ra::login(user->GetValue(),secret);std::fill(secret.begin(),secret.end(),0);}
            return;
        }
        if(action=="ra-logout"){ra::logout();render_detail();return;}
        if(action=="mods-refresh"){render_detail();return;}
        if(action=="mod-toggle"){
            for(const auto& m:gamebanana_installed())if(m.id==val){
                bool saved=false;
                if(m.textures){saved=gamebanana_enable_texture(m.id,!m.enabled);if(saved&&bindings.onModsInstalled)bindings.onModsInstalled();}
                else if(bindings.setSetting){
                    auto active=active_mods();const auto found=std::find(active.begin(),active.end(),m.id);
                    if(found==active.end())active.push_back(m.id);else active.erase(found);
                    std::string setting;for(const auto& id:active){if(!setting.empty())setting+=';';setting+=id;}
                    bindings.setSetting("mod",setting.empty()?"none":setting);saved=bindings.onSave&&bindings.onSave();
                }
                status_text(saved?(m.textures?"Textures mises à jour.":"Sélection enregistrée pour le prochain chargement de niveau."):"Modification du mod impossible.");render_detail();break;
            }return;
        }
        if(action=="banana-refresh"||action=="banana-page"){gamebanana_catalogue(action=="banana-page"?std::atoi(val.c_str()):1);render_detail();return;}
        if(action=="banana-files"){gamebanana_files(std::atoi(val.c_str()));render_detail();return;}
        if(action=="banana-install"){gamebanana_install(gamebanana_status().selected_mod,std::atoi(val.c_str()));render_detail();return;}
        if(action=="tab"){select_page(std::atoi(val.c_str()),true);return;}
        if(action=="row"){select_row(std::atoi(val.c_str()),false);return;}
        if(action=="resume"){resume();return;}
        if(action=="save"){status_text(bindings.onSave&&bindings.onSave()?"Réglages enregistrés.":"Enregistrement des réglages indisponible ou échoué.");return;}
        if(action=="quit"){if(bindings.onQuit)bindings.onQuit();return;}
        if(selected<0||selected>=(int)rows.size())return;
        const Row& row=rows[selected];
        if(action=="choice"){for(const Choice& c:row.choices)if(c.value==val){write_value(val);break;}return;}
        if((action=="decrease"||action=="increase")&&row.numeric){int n=std::atoi(value(row).c_str());n+=action=="increase"?row.step:-row.step;write_value(std::to_string(std::max(row.minimum,std::min(row.maximum,n))),action);return;}
        if(action=="capture"||action=="add-key"){capture=row.binding;add_key=action=="add-key";render_detail();return;}
        if(action=="cancel-capture"){capture=-1;render_detail();return;}
        if(action=="reset-binding"&&row.binding>=0&&bindings.setBinding){bindings.setBinding(row.binding,default_keys[row.binding]);render_rows();render_detail();return;}
        if(action=="backup"){ServiceResult r=backup_saves();status_text(r.message+(r.path.empty()?"":" "+r.path));return;}
        if(action=="refresh-backups"){backups=save_backups();render_detail();return;}
        if(action=="restore"){int i=std::atoi(val.c_str());if(i>=0&&i<(int)backups.size()){ServiceResult r=prepare_save_restore(backups[i]);status_text(r.message);}return;}
        if(action=="check-update"){check_updates(installed_release());render_detail();return;}
        if(action=="download-update"){download_update();render_detail();return;}
        if(action=="install-update"){if(bindings.onSave && !bindings.onSave()){status_text("Enregistrement échoué ; installation annulée.");return;}ServiceResult r=prepare_update_installation(true);status_text(r.message);if(r.ok&&bindings.onQuit)bindings.onQuit();return;}
    }
    void ProcessEvent(Rml::Event& event) override {
        if(!shown)return;
        Rml::Element* target=event.GetTargetElement();
        if(event.GetType()=="keydown" && target && (target->GetTagName()=="input" || target->GetTagName()=="textarea")) return;
        if (event.GetType()=="mousescroll") {
            Rml::Element* hovered = target;
            while (hovered && hovered != element("tabs")) hovered = hovered->GetParentNode();
            if (hovered) {
                const float delta = event.GetParameter("wheel_delta_y", 0.0f);
                hovered->SetScrollLeft(hovered->GetScrollLeft() + delta * 64.0f);
                event.StopPropagation();
            }
            return;
        }
        while(target && !target->HasAttribute("data-action"))target=target->GetParentNode();
        if(event.GetType()=="focus"){
            if(target && target->GetAttribute<std::string>("data-action","")=="row"){
                int index=target->GetAttribute<int>("data-index",0);if(index!=selected)select_row(index,false);
            }return;
        }
        if(event.GetType()=="keydown"){
            int key=event.GetParameter<int>("key_identifier",0);
            if(key==Rml::Input::KI_ESCAPE){if(capture>=0){capture=-1;render_detail();}else resume();event.StopPropagation();return;}
            if(capture>=0)return;
            if (key >= Rml::Input::KI_1 && key <= Rml::Input::KI_8) {
                select_page(key - Rml::Input::KI_1, true);
                event.StopPropagation();
                return;
            }
            std::string action=target?target->GetAttribute<std::string>("data-action",""):"";
            if(key==Rml::Input::KI_PRIOR || key==Rml::Input::KI_NEXT){select_page(page+(key==Rml::Input::KI_NEXT?1:-1),true);event.StopPropagation();return;}
            if(action=="tab" && (key==Rml::Input::KI_LEFT||key==Rml::Input::KI_RIGHT)){select_page(page+(key==Rml::Input::KI_RIGHT?1:-1),true);event.StopPropagation();return;}
            if((action=="row"||action=="tab") && (key==Rml::Input::KI_UP||key==Rml::Input::KI_DOWN)){select_row(selected+(key==Rml::Input::KI_DOWN?1:-1),true);event.StopPropagation();return;}
            if(action=="row" && key==Rml::Input::KI_LEFT){if(auto* tab=element(("tab-"+std::to_string(page)).c_str()))tab->Focus();event.StopPropagation();return;}
            if(action=="row" && key==Rml::Input::KI_RIGHT){if(auto* detail=element("detail")){Rml::ElementList buttons;detail->GetElementsByTagName(buttons,"button");if(!buttons.empty())buttons[0]->Focus();}event.StopPropagation();return;}
            if(target && target->GetTagName()=="button" && key==Rml::Input::KI_LEFT){select_row(selected,true);event.StopPropagation();return;}
            if(target && target->GetTagName()=="button" && (key==Rml::Input::KI_UP||key==Rml::Input::KI_DOWN)){
                Rml::Element* parent=target->GetParentNode();bool in_detail=false;
                while(parent){if(parent==element("detail")){in_detail=true;break;}parent=parent->GetParentNode();}
                if(in_detail){Rml::ElementList buttons;element("detail")->GetElementsByTagName(buttons,"button");
                    for(size_t i=0;i<buttons.size();++i)if(buttons[i]==target){int next=((int)i+(key==Rml::Input::KI_DOWN?1:-1)+(int)buttons.size())%(int)buttons.size();buttons[next]->Focus();buttons[next]->ScrollIntoView();break;}
                    event.StopPropagation();return;}
            }
            if((key==Rml::Input::KI_RETURN||key==Rml::Input::KI_SPACE)&&target){
                if(action=="row"){if(auto* right=element("detail")){Rml::ElementList buttons;right->GetElementsByTagName(buttons,"button");if(!buttons.empty())buttons[0]->Focus();}}
                else dispatch(action,target->GetAttribute<std::string>("data-value",target->GetAttribute<std::string>("data-index","")));
                event.StopPropagation();return;
            }
        }
        if(event.GetType()=="click"&&target){dispatch(target->GetAttribute<std::string>("data-action",""),target->GetAttribute<std::string>("data-value",target->GetAttribute<std::string>("data-index","")));event.StopPropagation();}
    }
};
PartyBoardMenu::PartyBoardMenu(Rml::Context* c,const MenuBindings& b):impl(new Impl(c,b)){}
PartyBoardMenu::~PartyBoardMenu(){}
bool PartyBoardMenu::available()const{return impl->document!=nullptr;}
bool PartyBoardMenu::visible()const{return impl->shown;}
void PartyBoardMenu::show(){if(!impl->document)return;impl->shown=true;impl->document->Show();impl->select_page(impl->page,true);}
void PartyBoardMenu::hide(){if(!impl->document)return;impl->shown=false;impl->capture=-1;impl->document->Hide();}
void PartyBoardMenu::update(){
    if(!impl->shown)return;
    const auto installed=gamebanana_status();
    if(installed.state==BananaState::Installed && installed.installed_mod!=impl->last_install){
        impl->last_install=installed.installed_mod;
        if(impl->bindings.onModsInstalled)impl->bindings.onModsInstalled();
        if(impl->bindings.refreshMods)impl->bindings.availableMods=impl->bindings.refreshMods();
        if(impl->page==5)impl->render_detail();
    }
    if(impl->page==6){
        if(auto* e=impl->element("ra-status"))e->SetInnerRML(escape(ra::status()));
        if(impl->selected>=0 && impl->selected<(int)impl->rows.size() && impl->rows[impl->selected].key=="achievements"){
            const std::string signature=ra::status()+std::to_string(ra::achievements().size());
            if(signature!=impl->last_service_signature){impl->last_service_signature=signature;impl->render_detail();}
        }
    }
    if(impl->page==5 && impl->selected>=0 && impl->selected<(int)impl->rows.size() && impl->rows[impl->selected].key=="gamebanana"){
        const auto state=gamebanana_status();const std::string signature=state.message+std::to_string((int)state.state)+std::to_string(state.selected_mod);
        if(signature!=impl->last_service_signature){impl->last_service_signature=signature;impl->render_detail();}
    }
    if(impl->page==7){UpdateStatus u=update_status();std::string signature=std::to_string((int)u.state)+u.message;
        if(signature!=impl->last_update_signature){impl->last_update_signature=signature;impl->render_detail();}}
}
void PartyBoardMenu::handleEvent(Rml::Event& e){impl->ProcessEvent(e);}
bool PartyBoardMenu::waitingForBinding()const{return impl->shown&&impl->capture>=0;}
bool PartyBoardMenu::captureBinding(const std::string& key){
    if(!waitingForBinding())return false;
    if(key=="ESCAPE"){impl->capture=-1;impl->render_detail();return true;}
    if(key.empty()||!impl->bindings.setBinding)return true;
    std::string binding=key;
    if(impl->add_key&&impl->bindings.getBinding){std::string old=impl->bindings.getBinding(impl->capture);std::istringstream words(old);std::string part;bool exists=false;
        while(words>>part)exists=exists||part==key;binding=exists?old:(old.empty()?key:old+" "+key);}
    impl->bindings.setBinding(impl->capture,binding);impl->capture=-1;impl->render_rows();impl->render_detail();return true;
}
} // namespace sms_frontend
