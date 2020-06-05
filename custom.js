$( document ).ready(function() {
    //As doxygen doesn't allow a lot of customization, we'll be applying those dynamically ¯\_(ツ)_/¯.

    /**** Nav Menu ****/
    //Rotate arrows on menu selection
    var lastMenuDevelopTime = 0;
    var hideMenusDone = false;
    var renameModulesDone = false;
    var clickCount=0;
    $("body").on('DOMSubtreeModified', "#nav-tree", function() {
        //First level menu arrows
        $('#nav-tree-contents li > .children_ul > li > .item > a >.arrow:contains("►")').removeClass('arrow-open').addClass('arrow-closed');
        $('#nav-tree-contents li > .children_ul > li > .item > a >.arrow:contains("▼")').removeClass('arrow-closed').addClass('arrow-open');
        //Lower level arrows
        $('#nav-tree-contents > ul > li > .children_ul > li > .children_ul > li > .item > a >.arrow:contains("►")').removeClass('arrow-open').addClass('arrow-closed');
        $('#nav-tree-contents > ul > li > .children_ul > li > .children_ul > li > .item > a >.arrow:contains("▼")').removeClass('arrow-closed').addClass('arrow-open');
        //Develop menu on first level click
        var menuItem = $('#nav-tree-contents > ul > li > .children_ul > li > .selected');
        if(menuItem.length>0 && $(menuItem[0]).parent().find('.children_ul').length == 0 && lastMenuDevelopTime+100<new Date().getTime()){
            lastMenuDevelopTime = new Date().getTime(); //avoid infitite loop call
            $(menuItem[0]).find('> a').trigger("click");
        }
        //Develop menu on second level click
        var menuItem = $('#nav-tree-contents > ul > li > .children_ul > li > .children_ul > li > .selected');
        if(menuItem.length>0 && $(menuItem[0]).parent().find('.children_ul').length == 0 && lastMenuDevelopTime+100<new Date().getTime()){
            lastMenuDevelopTime = new Date().getTime(); //avoid infitite loop call
            $(menuItem[0]).find('> a').trigger("click");
        }
        //Hide undesired menus
        /*if(!hideMenusDone && $('#nav-tree-contents > ul > li > .children_ul > li').length>0){
            hideMenusDone = true;
            $('#nav-tree-contents > ul > li > .children_ul > li').each(function(){
                console.log(this)
                //Get menu name
                var hideChildSubMenu = false;
                var label = $(this).find(".item > .label > a");
                if(label.length == 0 ) return;
                var text = $(label[0]).text();
                console.log(text)
                //Decide if we hide child sub menu
                if(text == "Modules") hideChildSubMenu = true;
                //Skip if not hiding
                if(!hideChildSubMenu) return;
                //hide sub menu
                $(this).addClass("childSubMenuHidden")
            })
        }*/
        
        //Remove 'Module' from module names
        if(!renameModulesDone){
            $(".children_ul > li > .item > .label > a").each(function(){
                var title = $(this).text();
                if(title.endsWith(" Module")){
                    title = title.replace(" Module","");
                    $(this).text(title);
                    renameModulesDone = true;
                }
            });
        }
    });
    //Move searchbox
    $("#MSearchBox").detach().prependTo('#nav-tree-contents');
    $("#MSearchField").attr("autocomplete","off");

    /**** Titles ****/
    //Remove 'Reference' at the end of titles
    $(".title").each(function(){
        var title = $(this).contents().filter(function() { return this.nodeType === 3;}).text();
        if(title.endsWith(" Reference")){
            title = title.replace(" Reference", "");
            $(this).text(title);
        }
        if(title.endsWith(" Struct")){
            title = title.replace(" Struct", "");
            $(this).text(title);
        }
        if(title.endsWith(" Class")){
            title = title.replace(" Class", "");
            $(this).text(title);
        }
    });
    //Remove 'Reference' from page title 
    var title = document.title;
    if(title.includes("|")){
        var titleArray = title.split("|");
        if(titleArray[0].endsWith(" Reference ")){
            titleArray[0] = titleArray[0].replace(" Reference", "");
        }
        if(titleArray[0].endsWith(" Struct ")){
            titleArray[0] = titleArray[0].replace(" Struct", "");
        }
        if(titleArray[0].endsWith(" Class ")){
            titleArray[0] = titleArray[0].replace(" Class", "");
        }
        
        title = titleArray.join("|");
        var url = window.location.href;
        if(url.includes("python"))
            title.replace(" API Reference ", " Python API Reference ")
        document.title = title;
    }
    //Select C++ or python sub menu
    var url = window.location.href;
    if(url.includes("python")){
        $("#subMenuCpp").removeClass("nav-links-selected");
        $("#subMenuPython").addClass("nav-links-selected");
    }
    else {
        $("#subMenuCpp").addClass("nav-links-selected");
        $("#subMenuPython").removeClass("nav-links-selected");
    }
});