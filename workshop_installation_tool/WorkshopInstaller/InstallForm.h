#pragma once

namespace WorkshopInstaller {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;

	/// <summary>
	/// Summary for InstallForm
	/// </summary>
	public ref class InstallForm : public System::Windows::Forms::Form
	{
	public:
		static InstallForm^ GetInstance()
		{
			if (!instance)
				instance = gcnew InstallForm();

			return instance;
		}

		void LoadForm(void);
		void UnloadForm(void);

		InstallForm(void)
		{
			InitializeComponent();
			//
			//TODO: Add the constructor code here
			//
		}

	protected:
		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		~InstallForm()
		{
			if (components)
			{
				delete components;
			}
		}

	private:
		static InstallForm^ instance;

	private: System::Windows::Forms::Label^  InstallLabel;
	protected:

	private:
		/// <summary>
		/// Required designer variable.
		/// </summary>
		System::ComponentModel::Container ^components;

#pragma region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		void InitializeComponent(void)
		{
			System::ComponentModel::ComponentResourceManager^  resources = (gcnew System::ComponentModel::ComponentResourceManager(InstallForm::typeid));
			this->InstallLabel = (gcnew System::Windows::Forms::Label());
			this->SuspendLayout();
			// 
			// InstallLabel
			// 
			this->InstallLabel->AutoSize = true;
			this->InstallLabel->Location = System::Drawing::Point(130, 44);
			this->InstallLabel->Name = L"InstallLabel";
			this->InstallLabel->Size = System::Drawing::Size(205, 20);
			this->InstallLabel->TabIndex = 0;
			this->InstallLabel->Text = L"Processing workshop item...";
			// 
			// InstallForm
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(9, 20);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(478, 144);
			this->ControlBox = false;
			this->Controls->Add(this->InstallLabel);
			this->FormBorderStyle = System::Windows::Forms::FormBorderStyle::FixedDialog;
			this->Icon = (cli::safe_cast<System::Drawing::Icon^>(resources->GetObject(L"$this.Icon")));
			this->MaximizeBox = false;
			this->MinimizeBox = false;
			this->Name = L"InstallForm";
			this->SizeGripStyle = System::Windows::Forms::SizeGripStyle::Hide;
			this->StartPosition = System::Windows::Forms::FormStartPosition::CenterScreen;
			this->Text = L"Item management";
			this->TopMost = true;
			this->ResumeLayout(false);
			this->PerformLayout();

		}
#pragma endregion
	};
}
