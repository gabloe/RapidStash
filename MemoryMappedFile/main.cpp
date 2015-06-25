#include "DynamicMemoryMappedFile.h"
#include <iostream>
#include <string>

int main() {
	STORAGE::DynamicMemoryMappedFile myFile("test.txt");
	std::string data("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Turpis dignissim feugiat.Semper sem consequat sociosqu mattis molestie lectus eget condimentum felis, egestas luctus.Ligula curabitur suspendisse dictumst phasellus litora.Molestie vulputate fusce etiam torquent mauris...Montes aptent hac platea posuere vivamus. Donec enim Est bibendum in ultricies urna - torquent ut?Donec mollis hendrerit sed orci ullamcorper quisque bibendum - nisi tellus phasellus nullam velit et.Elementum justo est purus luctus euismod!Aliquam lectus metus blandit?Cubilia lacus duis porttitor molestie est facilisi, vel egestas iaculis vitae. Sem tincidunt curabitur hendrerit vulputate rhoncus nullam nascetur.Pulvinar dui potenti maecenas enim pretium purus - congue ac curae facilisi euismod inceptos nostra.Senectus ipsum lobortis natoque nulla ornare dictum class in - urna interdum nascetur!Semper risus porttitor purus ac tellus fringilla: arcu egestas euismod.Cubilia donec porta auctor hac conubia magna erat; feugiat velit. Tempus mollis Consequat justo pretium vestibulum dictum egestas!Senectus habitant cubilia praesent ipsum tincidunt faucibus pharetra mattis fringilla; varius mauris inceptos ut.Auctor sodales magna mus nullam.Netus cubilia nisl pharetra conubia orci facilisi nisi: dignissim feugiat arcu luctus eleifend euismod.Aliquet pharetra cum varius. Amet natoque dictum feugiat mauris luctus iaculis?Lacus ipsum quis himenaeos venenatis purus ac; lacinia nunc?Praesent placerat eget mi.Pulvinar justo facilisis class ac vulputate nec: erat interdum felis velit inceptos nam?Morbi aptent id urna. Montes scelerisque tincidunt fermentum tempus conubia orci - tellus platea torquent eu!Cubilia maecenas mollis hendrerit aliquet ornare interdum?Lobortis fermentum Aenean mattis vulputate cursus metus libero volutpat?Montes aliquam risus mus lacinia primis rutrum.Morbi mattis molestie mauris. Eros convallis molestie.Consectetur amet nulla quis laoreet nisl auctor faucibus dapibus aliquet congue; per fusce in eget?At conubia erat.Ad Laoreet viverra per id vulputate; nibh urna rhoncus libero.Sociis ante potenti maecenas at nisl placerat sed est congue mus: platea phasellus mauris nunc? Ridiculus dapibus non et.Diam ligula proin imperdiet mi blandit vivamus nostra.Semper ante nulla fermentum in...Consectetur penatibus Consequat duis ullamcorper molestie curae tellus cum vivamus?Ante natoque Himenaeos enim porttitor rhoncus: sollicitudin rutrum ut. Sociis praesent potenti convallis curabitur leo elementum faucibus dictumst eget; sagittis imperdiet tristique erat integre?Eros donec sem natoque curabitur consequat sit: luctus inceptos?Tincidunt leo aliquet augue mattis viverra: magnis suscipit urna adipiscing.Tincidunt dapibus venenatis dignissim velit sollicitudin.Potenti ipsum eleifend. Sociis consectetur dui praesent porta magnis non!Consectetur diam penatibus mollis placerat vestibulum purus commodo taciti eget egestas.Semper diam curabitur placerat duis sociosqu posuere.Ridiculus auctor hendrerit vestibulum tortor molestie magnis - magna dictumst posuere torquent turpis nascetur eleifend ut?Eros consectetur Diam ad elementum pharetra magna quisque commodo - interdum varius ut.");
	myFile.raw_write(data.c_str(), data.size(), 0);
	myFile.shutdown();
	return 0;
}