#pragma once
#include "Locale.h"

using namespace System::IO; //IOException

namespace WorkshopInstaller
{
	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;

	enum class DialogButtons
	{
		OK,
		OKCancel
	};

	public ref class DialogForm : public System::Windows::Forms::Form
	{
	public:
		static DialogForm^ GetInstance()
		{
			if (!instance)
				instance = gcnew DialogForm();

			return instance;
		}

		DialogForm(void);

		void InitializeForm(void);
		void SetButtonLayout(DialogButtons newLayout);
		void SetHeading(String^ text);
		void SetDescription(String^ text);
		void SetFooter(String^ text);

	protected:
		~DialogForm()
		{
			if (components)
			{
				delete components;
			}
		}

	private:
		static DialogForm^ instance;

	private: System::Windows::Forms::GroupBox^ GroupTop;
	private: System::Windows::Forms::GroupBox^ groupMiddle;
	private: System::Windows::Forms::GroupBox^ GroupBottom;

	public: System::Windows::Forms::Label^ TextStart;
	public: System::Windows::Forms::TextBox^ TextMain;
	public: System::Windows::Forms::Label^ TextEnd;

	private: System::Windows::Forms::Button^ ActionButton;
	private:
		System::Windows::Forms::Button^ CancelButton;
		System::ComponentModel::Container^ components;

#pragma region Windows Form Designer generated code
		void InitializeComponent(void)
		{
			System::ComponentModel::ComponentResourceManager^ resources = (gcnew System::ComponentModel::ComponentResourceManager(DialogForm::typeid));
			this->GroupTop = (gcnew System::Windows::Forms::GroupBox());
			this->TextStart = (gcnew System::Windows::Forms::Label());
			this->GroupBottom = (gcnew System::Windows::Forms::GroupBox());
			this->TextEnd = (gcnew System::Windows::Forms::Label());
			this->ActionButton = (gcnew System::Windows::Forms::Button());
			this->CancelButton = (gcnew System::Windows::Forms::Button());
			this->groupMiddle = (gcnew System::Windows::Forms::GroupBox());
			this->TextMain = (gcnew System::Windows::Forms::TextBox());
			this->Icon = (cli::safe_cast<System::Drawing::Icon^>(resources->GetObject(L"$this.Icon")));
			this->Text = L"File conflict";
			this->GroupTop->SuspendLayout();
			this->GroupBottom->SuspendLayout();
			this->groupMiddle->SuspendLayout();
			this->SuspendLayout();
			// 
			// GroupTop
			// 
			this->GroupTop->Controls->Add(this->TextStart);
			this->GroupTop->Dock = System::Windows::Forms::DockStyle::Top;
			this->GroupTop->Location = System::Drawing::Point(14, 12);
			this->GroupTop->Margin = System::Windows::Forms::Padding(6, 6, 6, 6);
			this->GroupTop->Name = L"GroupTop";
			this->GroupTop->Padding = System::Windows::Forms::Padding(6, 6, 6, 6);
			this->GroupTop->Size = System::Drawing::Size(1276, 112);
			this->GroupTop->TabIndex = 1;
			this->GroupTop->TabStop = false;
			// 
			// TextStart
			// 
			this->TextStart->Dock = System::Windows::Forms::DockStyle::Fill;
			this->TextStart->Location = System::Drawing::Point(6, 30);
			this->TextStart->Margin = System::Windows::Forms::Padding(4, 4, 4, 4);
			this->TextStart->Name = L"TextStart";
			this->TextStart->Size = System::Drawing::Size(1264, 76);
			this->TextStart->TabIndex = 2;
			this->TextStart->Text = L"Conflicting files found:";
			// 
			// GroupBottom
			// 
			this->GroupBottom->Controls->Add(this->TextEnd);
			this->GroupBottom->Controls->Add(this->ActionButton);
			this->GroupBottom->Controls->Add(this->CancelButton);
			this->GroupBottom->Dock = System::Windows::Forms::DockStyle::Bottom;
			this->GroupBottom->Location = System::Drawing::Point(14, 436);
			this->GroupBottom->Margin = System::Windows::Forms::Padding(6, 6, 6, 6);
			this->GroupBottom->Name = L"GroupBottom";
			this->GroupBottom->Padding = System::Windows::Forms::Padding(6, 6, 6, 6);
			this->GroupBottom->Size = System::Drawing::Size(1276, 108);
			this->GroupBottom->TabIndex = 0;
			this->GroupBottom->TabStop = false;
			// 
			// TextEnd
			// 
			this->TextEnd->Location = System::Drawing::Point(6, 31);
			this->TextEnd->Margin = System::Windows::Forms::Padding(4, 4, 4, 4);
			this->TextEnd->Name = L"TextEnd";
			this->TextEnd->Size = System::Drawing::Size(700, 87);
			this->TextEnd->TabIndex = 1;
			this->TextEnd->Text = L"Continue anyway\? (Do this only if you know what you\'re doing!)";
			// 
			// ActionButton
			// 
			this->ActionButton->DialogResult = System::Windows::Forms::DialogResult::OK;
			this->ActionButton->Dock = System::Windows::Forms::DockStyle::Right;
			this->ActionButton->Location = System::Drawing::Point(850, 30);
			this->ActionButton->Margin = System::Windows::Forms::Padding(4, 4, 4, 4);
			this->ActionButton->Name = L"ActionButton";
			this->ActionButton->Size = System::Drawing::Size(210, 72);
			this->ActionButton->TabIndex = 0;
			this->ActionButton->Text = L"OK";
			this->ActionButton->UseVisualStyleBackColor = true;
			this->ActionButton->Click += gcnew System::EventHandler(this, &DialogForm::ActionButton_Click);
			// 
			// CancelButton
			// 
			this->CancelButton->DialogResult = System::Windows::Forms::DialogResult::Cancel;
			this->CancelButton->Dock = System::Windows::Forms::DockStyle::Right;
			this->CancelButton->Location = System::Drawing::Point(1060, 30);
			this->CancelButton->Margin = System::Windows::Forms::Padding(4, 4, 4, 4);
			this->CancelButton->Name = L"CancelButton";
			this->CancelButton->Size = System::Drawing::Size(210, 72);
			this->CancelButton->TabIndex = 1;
			this->CancelButton->Text = L"Cancel";
			this->CancelButton->UseVisualStyleBackColor = true;
			this->CancelButton->Click += gcnew System::EventHandler(this, &DialogForm::button1_Click);
			// 
			// groupMiddle
			// 
			this->groupMiddle->Controls->Add(this->TextMain);
			this->groupMiddle->Dock = System::Windows::Forms::DockStyle::Fill;
			this->groupMiddle->Location = System::Drawing::Point(14, 124);
			this->groupMiddle->Margin = System::Windows::Forms::Padding(6, 6, 6, 6);
			this->groupMiddle->Name = L"groupMiddle";
			this->groupMiddle->Padding = System::Windows::Forms::Padding(6);
			this->groupMiddle->Size = System::Drawing::Size(1276, 312);
			this->groupMiddle->TabIndex = 2;
			this->groupMiddle->TabStop = false;
			// 
			// TextMain
			// 
			this->TextMain->Dock = System::Windows::Forms::DockStyle::Fill;
			this->TextMain->Location = System::Drawing::Point(6, 30);
			this->TextMain->Margin = System::Windows::Forms::Padding(4, 0, 4, 0);
			this->TextMain->Multiline = true;
			this->TextMain->Name = L"TextMain";
			this->TextMain->ReadOnly = true;
			this->TextMain->ScrollBars = System::Windows::Forms::ScrollBars::Vertical;
			this->TextMain->Size = System::Drawing::Size(1264, 276);
			this->TextMain->TabIndex = 1;
			// 
			// DialogForm
			// 
			this->AcceptButton = this->ActionButton;
			this->AutoScaleDimensions = System::Drawing::SizeF(12, 25);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(1304, 556);
			this->Controls->Add(this->groupMiddle);
			this->Controls->Add(this->GroupBottom);
			this->Controls->Add(this->GroupTop);
			this->Margin = System::Windows::Forms::Padding(4, 4, 4, 4);
			this->Name = L"DialogForm";
			this->Padding = System::Windows::Forms::Padding(14, 12, 14, 12);
			this->ShowIcon = true;
			this->ShowInTaskbar = false;
			this->SizeGripStyle = System::Windows::Forms::SizeGripStyle::Hide;
			this->StartPosition = System::Windows::Forms::FormStartPosition::CenterScreen;
			this->GroupTop->ResumeLayout(false);
			this->GroupBottom->ResumeLayout(false);
			this->groupMiddle->ResumeLayout(false);
			this->groupMiddle->PerformLayout();
			this->ResumeLayout(false);

		}
#pragma endregion
	private:
		System::Void ActionButton_Click(System::Object^ sender, System::EventArgs^ e)
		{
		}
	private:
		System::Void button1_Click(System::Object^ sender, System::EventArgs^ e)
		{
		}
	};
};
