jQuery( document ).ready(function() {
    var elems = jQuery('.renew-loan-button').find('input');
    elems.on('click',function(){
         jQuery('<div class="search-overlay--wrapper"><div class="search-overlay--inner"><i class="icon-spinner icon-spin search-overlay--icon"></i><p class="search-overlay--text">' + Drupal.t('Fornyer dine materialer...') + '</p><p class="cancel"><a href="#">' + Drupal.t('Cancel') + '</a></p></div></div>').prependTo('body');
    });
});


