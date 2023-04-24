#include <curl/curl.h>
#include "events.h"
#include <syslog.h>
#include <string.h>
#include "uci_utils.h"
#include "mqtt_utils.h"

#define EMAIL_LEN 1024
char email_msg[EMAIL_LEN];
/*Creates an email message string*/
int form_email_message(char* dest, char* content, char *from){
    //printf("From: %s %s\n", from, content);
    snprintf(dest, EMAIL_LEN, "From: MQTT subscriber <%s>\r\nTo: %s\r\nSubject: MQTT Event\r\n\r\n%s\r\n", from, "user", content );
}
struct upload_status {
    size_t bytes_read;
};

size_t payload_source(char *ptr, size_t size, size_t nmemb, void *userp)
{
    struct upload_status *upload_ctx = (struct upload_status *)userp;
    char *data;
    size_t room = size * nmemb;
    
    if((size == 0) || (nmemb == 0) || ((size*nmemb) < 1)) {
        return 0;
    }
 
    data = &email_msg[upload_ctx->bytes_read];
    
    if(data) {
        size_t len = strlen(data);
        if(room < len)
        len = room;
        memcpy(ptr, data, len);
        upload_ctx->bytes_read += len;
    
        return len;
    }
    return 0;
}
/*Sets CURL user and SSL options*/
void curl_set_sender_data(CURL *curl, struct sender sender)
{
    if(sender.credentials){
        curl_easy_setopt(curl, CURLOPT_USERNAME, sender.user);
        curl_easy_setopt(curl, CURLOPT_PASSWORD, sender.password);
        curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
        curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/cacert.pem");
    }
    char url[MAX_STR_LEN];
    
    sprintf(url, "%s:%d", sender.smtp_ip, sender.port);
    curl_easy_setopt(curl, CURLOPT_URL, sender.smtp_ip);
    //curl_easy_setopt(curl, CURLOPT_PORT, (long)sender.port);

    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, sender.email);
}
/*Sets CURL message receivers options*/
void curl_set_receivers_data(CURL *curl, struct curl_slist *recipients, char addresses[][MAX_STR_LEN])
{

    for(int i = 0; i < MAX_RECEIVERS; i ++){
        if(strcmp(addresses[i], "") == 0)
            break;
        recipients = curl_slist_append(recipients, addresses[i]);
    }
    
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
}
/*Sends an email to everyone in ADDRESSES from SENDER_MAIL
The sent email contents are defined by MESSAGE*/
int send_email(char *sender_mail, char addresses[][MAX_STR_LEN], char *message)
{
    form_email_message(email_msg, message, sender_mail);
    struct sender sender = {0};
    
    if(get_sender_data(&sender, sender_mail) != 0){
        return SENDER_DATA_ERR;
    }  
    
    CURL *curl;
    CURLcode ret = CURLE_OK;
    struct curl_slist *recipients = NULL;
    struct upload_status upload_ctx = { 0 };
    
    curl = curl_easy_init();
    if(curl){
        
        curl_set_sender_data(curl, sender);
        curl_set_receivers_data(curl, recipients, addresses);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
        curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        ret = curl_easy_perform(curl);

        if(ret != CURLE_OK){
            syslog(LOG_ERR, "Failed to send email: %s (%d)", curl_easy_strerror(ret), ret);
        } else{
            syslog(LOG_INFO, "Email sent.");
        }
    
        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
    }
    return ret;
}
